#include "game/runtime/RuntimeGameSession.h"

#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/modules/checkpoint/CheckpointFeedbackState.h"
#include "game/modules/checkpoint/CheckpointSystem.h"
#include "game/modules/door/DoorComponents.h"
#include "game/modules/interaction/InteractionSystem.h"
#include "game/modules/player_control/PlayerControlCamera.h"
#include "game/modules/player_control/PlayerControlMovement.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/levels/ProceduralGameAssets.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/modules/door/DoorAnimationSystem.h"
#include "game/modules/inventory/InventoryCaptureState.h"
#include "game/modules/inventory/InventoryMenuState.h"
#include "game/modules/inventory/InventorySystem.h"
#include "game/systems/KinematicColliderSystem.h"
#include "game/session/RunSession.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>

struct RuntimeMutableSnapshot {
    struct PlayerState {
        entt::entity entity = entt::null;
        TransformComponent transform{};
        PlayerMovementComponent movement{};
        PlayerInteractionLockComponent interactionLock{};
        PlayerSpawnComponent spawn{};
        bool valid = false;

        // Camera baseline (captured from CameraManager)
        float cameraYaw = -90.0f;
        float cameraPitch = 0.0f;
        float cameraFov = 70.0f;
    };

    RunSession runSession{};
    PlayerState player{};
    std::vector<std::pair<entt::entity, DoorStateComponent>> doors;
    std::vector<std::pair<entt::entity, CheckpointComponent>> checkpoints;
};

namespace {

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void bootstrapRuntimeMeshLibrary(MeshLibrary& meshLibrary) {
    registerProceduralAssets(meshLibrary);
    const std::filesystem::path relativeBase(resolveProjectPath("."));

    const std::filesystem::path meshDirectory(resolveProjectPath("assets/meshes"));
    for (const auto& asset : ModelLoader::discoverProjectAssets(meshDirectory, relativeBase)) {
        if (!meshLibrary.has(asset.meshId)) {
            meshLibrary.registerFileAlias(asset.meshId, asset.relativePath);
        }
    }

    const std::filesystem::path packsDirectory(resolveProjectPath("assets/packs"));
    for (const auto& asset : ModelLoader::discoverProjectAssets(packsDirectory, relativeBase)) {
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
                                 const LevelLoadRequest& request,
                                 bool contentChanged) {
    const Clock::time_point start = Clock::now();
    performanceStats_.resetForPlayMs = 0.0;
    content_ = &content;
    ensureInitialized();
    if (rendererInitialized_ && contentChanged) {
        renderer_.reloadContent(content);
    }
    clearEntities();
    runSession_ = RunSession{};
    elapsedTime_ = 0.0f;
    behaviors_.reset();
    inputSystem_.reset();

    registry_.ctx().insert_or_assign<ContentRegistry*>(&content);
    registry_.ctx().insert_or_assign<RunSession*>(&runSession_);
    registry_.ctx().insert_or_assign<PhysicsSystem*>(&physics_);
    registry_.ctx().insert_or_assign<CameraManager*>(&cameraManager_);

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
    initializeCheckpointFeedback(registry_);
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

    elapsedTime_ = 0.0f;
    behaviors_.reset();
    restoreBaselineState();
    resetDoorVisuals(registry_);
    resetTransientRuntimeState();
    initializeRuntimeInteraction(registry_);
    initializeRuntimeInventory(registry_);
    initializeCheckpointFeedback(registry_);
    tickCheckpointFeedback(registry_, 0.0f);
    physics_.update(registry_, 0.0f);

    performanceStats_.resetForPlayMs = elapsedMilliseconds(start, Clock::now());
}

void RuntimeGameSession::tick(float deltaTime, float aspect) {
    if (!physicsInitialized_) {
        return;
    }

    elapsedTime_ += deltaTime;

    const Clock::time_point tickStart = Clock::now();

    Clock::time_point t0 = Clock::now();
    updateRuntimeInteraction(registry_, inputSystem_, cameraManager_);
    Clock::time_point t1 = Clock::now();
    performanceStats_.interactionMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    tickCheckpointFeedback(registry_, deltaTime);
    t1 = Clock::now();
    performanceStats_.checkpointsMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    behaviors_.tick(registry_, elapsedTime_, eventSink_);
    tickDoorAnimation(registry_, deltaTime);
    tickKinematicColliders(registry_, physics_, deltaTime);
    t1 = Clock::now();

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

    t0 = t1;
    tickPlayerMovement(registry_, inputSystem_, cameraManager_, physics_, deltaTime);
    t1 = Clock::now();
    performanceStats_.movementMs = elapsedMilliseconds(t0, t1);

    t0 = t1;
    tickPlayerCamera(registry_, inputSystem_, cameraManager_, aspect, deltaTime);

    auto& ctrl = debugParams_.cameraControl;
    if (ctrl.triggerShake) {
        cameraManager_.shake(ctrl.shakeTrauma);
        ctrl.triggerShake = false;
    }
    if (ctrl.triggerFOV) {
        cameraManager_.punchFOV(ctrl.fovDelta, ctrl.fovDuration);
        ctrl.triggerFOV = false;
    }
    if (ctrl.triggerTransition) {
        const glm::vec3 target(ctrl.targetPosition[0], ctrl.targetPosition[1], ctrl.targetPosition[2]);
        cameraManager_.transitionTo(target, ctrl.targetYaw, ctrl.targetPitch,
                                    ctrl.transitionDuration);
        ctrl.triggerTransition = false;
    }
    if (ctrl.triggerReturn) {
        const auto& base = cameraManager_.getBaseState();
        cameraManager_.transitionTo(base.position, base.yaw, base.pitch, ctrl.transitionDuration);
        ctrl.triggerReturn = false;
    }

    cameraManager_.update(deltaTime);

    ctrl.isTransitioning = cameraManager_.isTransitioning();
    ctrl.currentTrauma = cameraManager_.currentTrauma();

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
    cameraManager_.setBaseState(position, yaw, std::clamp(pitch, -89.0f, 89.0f));
    if (fov.has_value()) {
        const auto& base = cameraManager_.getBaseState();
        cameraManager_.setProjection(fov.value(), 16.0f / 9.0f, base.nearPlane, base.farPlane);
    }

    auto view = registry_.view<TransformComponent, PlayerTag>();
    for (auto [entity, transform] : view.each()) {
        transform.position = position;

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
                     cameraManager_.getState(),
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

    auto playerView = registry_.view<TransformComponent, PlayerMovementComponent,
                                     PlayerInteractionLockComponent, PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, transform, movement, lock, spawn] : playerView.each()) {
        baselineSnapshot_->player.entity = entity;
        baselineSnapshot_->player.transform = transform;
        baselineSnapshot_->player.movement = movement;

        const auto& baseCam = cameraManager_.getBaseState();
        baselineSnapshot_->player.cameraYaw = baseCam.yaw;
        baselineSnapshot_->player.cameraPitch = baseCam.pitch;
        baselineSnapshot_->player.cameraFov = baseCam.fov;
        baselineSnapshot_->player.interactionLock = lock;
        baselineSnapshot_->player.spawn = spawn;
        baselineSnapshot_->player.valid = true;
        break;
    }

    auto doorView = registry_.view<DoorConfigComponent, DoorStateComponent>();
    for (auto [entity, config, state] : doorView.each()) {
        (void)config;
        baselineSnapshot_->doors.emplace_back(entity, state);
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
        cameraManager_.setBaseState(baselineSnapshot_->player.transform.position,
                                    baselineSnapshot_->player.cameraYaw,
                                    baselineSnapshot_->player.cameraPitch);
        {
            const auto& base = cameraManager_.getBaseState();
            cameraManager_.setProjection(baselineSnapshot_->player.cameraFov,
                                         16.0f / 9.0f, base.nearPlane, base.farPlane);
        }
        cameraManager_.clearEffects();
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

    for (const auto& [entity, state] : baselineSnapshot_->doors) {
        if (registry_.valid(entity) && registry_.all_of<DoorStateComponent>(entity)) {
            registry_.patch<DoorStateComponent>(entity, [&](auto& component) {
                component = state;
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
    resetRuntimeInteraction(registry_);
    resetRuntimeInventory(registry_);
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
    clearRuntimeInventory(registry_);
    clearRuntimeInteraction(registry_);
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
    if (ctx.contains<PhysicsSystem*>()) {
        ctx.erase<PhysicsSystem*>();
    }
    if (ctx.contains<CameraManager*>()) {
        ctx.erase<CameraManager*>();
    }
    cameraManager_.clearEffects();
    baselineSnapshot_.reset();
}
