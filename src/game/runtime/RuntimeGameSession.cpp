#include "game/runtime/RuntimeGameSession.h"

#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"
#include "game/components/CameraComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/levels/ProceduralGameAssets.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/session/RunSession.h"
#include "game/ui/InteractionFocusState.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>

struct RuntimeMutableSnapshot {
    struct PlayerState {
        entt::entity entity = entt::null;
        TransformComponent transform{};
        CameraComponent camera{};
        PlayerMovementComponent movement{};
        PlayerInteractionLockComponent interactionLock{};
        PlayerSpawnComponent spawn{};
        bool valid = false;
    };

    RunSession runSession{};
    PlayerState player{};
    std::vector<std::pair<entt::entity, DoorComponent>> doors;
    std::vector<std::pair<entt::entity, CheckpointComponent>> checkpoints;
};

namespace {

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void bootstrapRuntimeMeshLibrary(MeshLibrary& meshLibrary) {
    registerProceduralAssets(meshLibrary);

    const std::filesystem::path meshDirectory(resolveProjectPath("assets/meshes"));
    for (const auto& asset : ModelLoader::discoverProjectAssets(meshDirectory, std::filesystem::current_path())) {
        if (!meshLibrary.has(asset.meshId)) {
            meshLibrary.registerFileAlias(asset.meshId, asset.relativePath);
        }
    }

    const std::filesystem::path packsDirectory(resolveProjectPath("assets/packs"));
    for (const auto& asset : ModelLoader::discoverProjectAssets(packsDirectory, std::filesystem::current_path())) {
        if (!meshLibrary.has(asset.meshId)) {
            meshLibrary.registerFileAlias(asset.meshId, asset.relativePath);
        }
    }
}

void registerGameActions(InputSystem& input) {
    auto& actions = input.actionMap();
    actions.bind("move_forward",  ActionBinding{.keys = {GLFW_KEY_W}});
    actions.bind("move_backward", ActionBinding{.keys = {GLFW_KEY_S}});
    actions.bind("strafe_left",   ActionBinding{.keys = {GLFW_KEY_A}});
    actions.bind("strafe_right",  ActionBinding{.keys = {GLFW_KEY_D}});
    actions.bind("sprint",        ActionBinding{.keys = {GLFW_KEY_LEFT_SHIFT}});
    actions.bind("jump",          ActionBinding{.keys = {GLFW_KEY_SPACE}});
    actions.bind("interact",      ActionBinding{.keys = {GLFW_KEY_E}});
    actions.bind("inventory",     ActionBinding{.keys = {GLFW_KEY_I}});
    actions.bind("attack",        ActionBinding{.mouseButtons = {GLFW_MOUSE_BUTTON_LEFT}});
    actions.bind("screenshot",    ActionBinding{.keys = {GLFW_KEY_R}});
}

} // namespace

RuntimeGameSession::RuntimeGameSession() {
    bootstrapRuntimeMeshLibrary(meshLibrary_);
    registerGameActions(inputSystem_);
}

RuntimeGameSession::~RuntimeGameSession() {
    clear();
    if (rendererInitialized_) {
        renderer_.shutdown();
    }
    if (physicsInitialized_) {
        physics_.shutdownRuntime();
    }
}

void RuntimeGameSession::ensureInitialized() {
    if (physicsInitialized_) {
        return;
    }

    physics_.init(registry_);
    physicsInitialized_ = true;
}

void RuntimeGameSession::ensureRendererInitialized() {
    if (rendererInitialized_ || content_ == nullptr) {
        return;
    }

    const Clock::time_point start = Clock::now();
    renderer_.init(*content_);
    performanceStats_.rendererInitMs = elapsedMilliseconds(start, Clock::now());
    rendererInitialized_ = true;
}

void RuntimeGameSession::rebuild(const LevelDef& level,
                                 const std::string& levelId,
                                 const std::string& levelPath,
                                 ContentRegistry& content,
                                 const LevelLoadRequest& request) {
    const Clock::time_point start = Clock::now();
    performanceStats_.resetForPlayMs = 0.0;
    content_ = &content;
    ensureInitialized();
    if (rendererInitialized_) {
        renderer_.reloadContent(content);
    }
    clearEntities();
    runSession_ = RunSession{};
    inputSystem_.reset();

    registry_.ctx().insert_or_assign<ContentRegistry*>(&content);
    registry_.ctx().insert_or_assign<RunSession*>(&runSession_);

    LevelBuildContext context{
        .registry = registry_,
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    LevelLoader loader(context);
    LevelLoadRequest resolvedRequest = request;
    resolvedRequest.levelId = levelId;
    resolvedRequest.levelPath = levelPath;
    LevelLoadArgs args{&content, &runSession_, &level};
    loader.load(resolvedRequest, args);

    initializeRuntimeInteraction(registry_);
    initializeRuntimeInventory(registry_);
    initializeRuntimeCheckpoints(registry_);
    physics_.update(registry_, 0.0f);
    captureBaselineState();
    performanceStats_.rebuildMs = elapsedMilliseconds(start, Clock::now());
}

void RuntimeGameSession::clear() {
    clearEntities();
    inputSystem_.reset();
}

void RuntimeGameSession::resetForPlay() {
    const Clock::time_point start = Clock::now();
    performanceStats_.rebuildMs = 0.0;
    if (!baselineSnapshot_) {
        performanceStats_.resetForPlayMs = 0.0;
        return;
    }

    restoreBaselineState();
    resetTransientRuntimeState();
    initializeRuntimeInteraction(registry_);
    initializeRuntimeInventory(registry_);
    initializeRuntimeCheckpoints(registry_);
    updateRuntimeCheckpoints(registry_, 0.0f, runSession_);
    physics_.update(registry_, 0.0f);

    performanceStats_.resetForPlayMs = elapsedMilliseconds(start, Clock::now());
}

void RuntimeGameSession::tick(float deltaTime, float aspect) {
    if (!physicsInitialized_) {
        return;
    }

    const Clock::time_point tickStart = Clock::now();

    Clock::time_point t0 = Clock::now();
    updateRuntimeInteraction(registry_, inputSystem_);
    Clock::time_point t1 = Clock::now();
    performanceStats_.interactionMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    updateRuntimeCheckpoints(registry_, deltaTime, runSession_);
    t1 = Clock::now();
    performanceStats_.checkpointsMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    physics_.update(registry_, deltaTime);
    t1 = Clock::now();
    performanceStats_.physicsMs = elapsedMilliseconds(t0, t1);

    ContentRegistry* content = nullptr;
    if (registry_.ctx().contains<ContentRegistry*>()) {
        content = registry_.ctx().get<ContentRegistry*>();
    }
    t0 = Clock::now();
    if (content != nullptr) {
        updateRuntimeInventory(registry_, inputSystem_, runSession_, *content);
    }
    t1 = Clock::now();
    performanceStats_.inventoryMs = elapsedMilliseconds(t0, t1);

    updateRuntimeBehaviors(registry_);
    updateRuntimeDoorAnimation(registry_, deltaTime);

    t0 = t1;
    updateRuntimePlayerMovement(registry_, inputSystem_, physics_, deltaTime);
    t1 = Clock::now();
    performanceStats_.movementMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    updateRuntimeCamera(registry_, inputSystem_, aspect, deltaTime);
    t1 = Clock::now();
    performanceStats_.cameraMs = elapsedMilliseconds(t0, t1);

    performanceStats_.totalTickMs = elapsedMilliseconds(tickStart, t1);
}

void RuntimeGameSession::prewarmRenderer(ContentRegistry& content) {
    const Clock::time_point start = Clock::now();
    content_ = &content;
    if (rendererInitialized_) {
        renderer_.reloadContent(content);
    }
    ensureRendererInitialized();
    if (rendererInitialized_) {
        (void)renderer_.prewarmMaterialResources(registry_);
    }
    performanceStats_.rendererPrewarmMs = elapsedMilliseconds(start, Clock::now());
}

bool RuntimeGameSession::setPrimaryCameraView(const glm::vec3& position,
                                              float yaw,
                                              float pitch,
                                              const std::optional<float>& fov) {
    auto view = registry_.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : view.each()) {
        transform.position = position;
        camera.yaw = yaw;
        camera.pitch = std::clamp(pitch, -89.0f, 89.0f);
        if (fov.has_value()) {
            camera.fov = fov.value();
        }

        if (registry_.all_of<PlayerMovementComponent>(entity)) {
            auto& movement = registry_.get<PlayerMovementComponent>(entity);
            movement.velocity = glm::vec3(0.0f);
            movement.grounded = false;
            movement.jumpHeld = false;
            movement.jumpHoldTimer = 0.0f;
        }

        if (registry_.all_of<CharacterControllerComponent>(entity)) {
            const auto& controller = registry_.get<CharacterControllerComponent>(entity);
            physics_.setCharacterVelocity(entity, glm::vec3(0.0f));
            physics_.setCharacterPosition(entity,
                                          position - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));
        }

        return true;
    }

    return false;
}

void RuntimeGameSession::setEnvironmentOverride(const EnvironmentDefinition& definition) {
    registry_.ctx().insert_or_assign<RuntimeEnvironmentOverride>(RuntimeEnvironmentOverride{definition});
    environmentSyncState_ = RuntimeEnvironmentSyncState{};
}

void RuntimeGameSession::clearEnvironmentOverride() {
    auto& ctx = registry_.ctx();
    if (ctx.contains<RuntimeEnvironmentOverride>()) {
        ctx.erase<RuntimeEnvironmentOverride>();
    }
    environmentSyncState_ = RuntimeEnvironmentSyncState{};
}

RuntimeSceneRenderOutput RuntimeGameSession::render(float deltaTime,
                                                    int internalWidth,
                                                    int internalHeight,
                                                    int outputWidth,
                                                    int outputHeight,
                                                    GLuint targetFramebuffer) {
    const Clock::time_point start = Clock::now();
    RuntimeSceneRenderOutput output;
    if (!physicsInitialized_) {
        performanceStats_.lastRenderMs = elapsedMilliseconds(start, Clock::now());
        return output;
    }
    ensureRendererInitialized();
    if (!rendererInitialized_) {
        performanceStats_.lastRenderMs = elapsedMilliseconds(start, Clock::now());
        return output;
    }
    renderer_.render(registry_,
                     debugParams_,
                     deltaTime,
                     internalWidth,
                     internalHeight,
                     outputWidth,
                     outputHeight,
                     targetFramebuffer,
                     &environmentSyncState_,
                     true,
                     &output);
    performanceStats_.lastRenderMs = elapsedMilliseconds(start, Clock::now());
    return output;
}

void RuntimeGameSession::captureBaselineState() {
    baselineSnapshot_ = std::make_unique<RuntimeMutableSnapshot>();
    baselineSnapshot_->runSession = runSession_;
    baselineSnapshot_->doors.clear();
    baselineSnapshot_->checkpoints.clear();

    auto playerView = registry_.view<TransformComponent, CameraComponent, PlayerMovementComponent,
                                     PlayerInteractionLockComponent, PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, transform, camera, movement, lock, spawn] : playerView.each()) {
        baselineSnapshot_->player.entity = entity;
        baselineSnapshot_->player.transform = transform;
        baselineSnapshot_->player.camera = camera;
        baselineSnapshot_->player.movement = movement;
        baselineSnapshot_->player.interactionLock = lock;
        baselineSnapshot_->player.spawn = spawn;
        baselineSnapshot_->player.valid = true;
        break;
    }

    auto doorView = registry_.view<DoorComponent>();
    for (auto [entity, door] : doorView.each()) {
        baselineSnapshot_->doors.emplace_back(entity, door);
    }

    auto checkpointView = registry_.view<CheckpointComponent>();
    for (auto [entity, checkpoint] : checkpointView.each()) {
        baselineSnapshot_->checkpoints.emplace_back(entity, checkpoint);
    }
}

void RuntimeGameSession::restoreBaselineState() {
    if (!baselineSnapshot_) {
        return;
    }

    runSession_ = baselineSnapshot_->runSession;
    registry_.ctx().insert_or_assign<RunSession*>(&runSession_);
    inputSystem_.reset();
    environmentSyncState_ = RuntimeEnvironmentSyncState{};

    if (baselineSnapshot_->player.valid && registry_.valid(baselineSnapshot_->player.entity)) {
        const entt::entity player = baselineSnapshot_->player.entity;
        registry_.patch<TransformComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.transform;
        });
        registry_.patch<CameraComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.camera;
        });
        registry_.patch<PlayerMovementComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.movement;
        });
        registry_.patch<PlayerInteractionLockComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.interactionLock;
        });
        registry_.patch<PlayerSpawnComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.spawn;
        });

        if (registry_.all_of<CharacterControllerComponent>(player)) {
            const auto& controller = registry_.get<CharacterControllerComponent>(player);
            physics_.setCharacterVelocity(player, glm::vec3(0.0f));
            physics_.setCharacterPosition(player,
                                          baselineSnapshot_->player.transform.position
                                              - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));
        }
    }

    for (const auto& [entity, door] : baselineSnapshot_->doors) {
        if (registry_.valid(entity) && registry_.all_of<DoorComponent>(entity)) {
            registry_.patch<DoorComponent>(entity, [&](auto& component) {
                component = door;
            });
        }
    }

    for (const auto& [entity, checkpoint] : baselineSnapshot_->checkpoints) {
        if (registry_.valid(entity) && registry_.all_of<CheckpointComponent>(entity)) {
            registry_.patch<CheckpointComponent>(entity, [&](auto& component) {
                component = checkpoint;
            });
        }
    }
}

void RuntimeGameSession::resetTransientRuntimeState() {
    auto& ctx = registry_.ctx();
    if (ctx.contains<InteractionPromptState>()) {
        ctx.insert_or_assign<InteractionPromptState>(InteractionPromptState{});
    }
    if (ctx.contains<InteractionFocusState>()) {
        ctx.insert_or_assign<InteractionFocusState>(InteractionFocusState{});
    }
    if (ctx.contains<InventoryMenuState>()) {
        ctx.insert_or_assign<InventoryMenuState>(InventoryMenuState{});
    }
    if (ctx.contains<RuntimeInventoryCaptureState>()) {
        ctx.insert_or_assign<RuntimeInventoryCaptureState>(RuntimeInventoryCaptureState{});
    }
    if (ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.insert_or_assign<RuntimeCheckpointFeedbackState>(RuntimeCheckpointFeedbackState{});
    }
}

void RuntimeGameSession::clearEntities() {
    for (auto entity : entities_) {
        if (registry_.valid(entity)) {
            registry_.destroy(entity);
        }
    }
    entities_.clear();

    auto& ctx = registry_.ctx();
    if (ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.erase<RuntimeCheckpointFeedbackState>();
    }
    if (ctx.contains<RuntimeInventoryCaptureState>()) {
        ctx.erase<RuntimeInventoryCaptureState>();
    }
    if (ctx.contains<InventoryMenuState>()) {
        ctx.erase<InventoryMenuState>();
    }
    if (ctx.contains<InteractionPromptState>()) {
        ctx.erase<InteractionPromptState>();
    }
    if (ctx.contains<InteractionFocusState>()) {
        ctx.erase<InteractionFocusState>();
    }
    if (ctx.contains<MeshAssetProvider>()) {
        ctx.erase<MeshAssetProvider>();
    }
    if (ctx.contains<ActiveEnvironmentProfile>()) {
        ctx.erase<ActiveEnvironmentProfile>();
    }
    if (ctx.contains<RuntimeEnvironmentOverride>()) {
        ctx.erase<RuntimeEnvironmentOverride>();
    }
    if (ctx.contains<ContentRegistry*>()) {
        ctx.erase<ContentRegistry*>();
    }
    if (ctx.contains<RunSession*>()) {
        ctx.erase<RunSession*>();
    }
    baselineSnapshot_.reset();
}
