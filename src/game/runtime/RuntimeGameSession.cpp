#include "game/runtime/RuntimeGameSession.h"

#include "engine/core/PathUtils.h"
#include "game/content/ContentRegistry.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/ui/InteractionFocusState.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <filesystem>

namespace {

void bootstrapRuntimeMeshLibrary(MeshLibrary& meshLibrary) {
    registerCathedralAssets(meshLibrary);

    const std::filesystem::path meshDirectory(resolveProjectPath("assets/meshes"));
    if (std::filesystem::exists(meshDirectory)) {
        for (const auto& entry : std::filesystem::directory_iterator(meshDirectory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string extension = entry.path().extension().string();
            if (extension != ".glb" && extension != ".gltf") {
                continue;
            }
            const std::string meshId = entry.path().stem().string();
            if (!meshLibrary.has(meshId)) {
                meshLibrary.registerFileAlias(meshId, std::filesystem::relative(entry.path(), std::filesystem::current_path()).generic_string());
            }
        }
    }

    const std::string staticDoorPath = resolveProjectPath("assets/meshes/gothic_door_static.glb");
    if (std::filesystem::exists(staticDoorPath) && !meshLibrary.has("gothic_door_static")) {
        meshLibrary.registerFileAlias("gothic_door_static", "assets/meshes/gothic_door_static.glb");
    }
}

} // namespace

RuntimeGameSession::RuntimeGameSession() {
    bootstrapRuntimeMeshLibrary(meshLibrary_);
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

    renderer_.init(*content_);
    rendererInitialized_ = true;
}

void RuntimeGameSession::rebuild(const LevelDef& level,
                                 const std::string& levelId,
                                 const std::string& levelPath,
                                 ContentRegistry& content,
                                 const LevelLoadRequest& request) {
    content_ = &content;
    ensureInitialized();
    if (rendererInitialized_) {
        renderer_.reloadContent(content);
    }
    clearEntities();
    runSession_ = RunSession{};
    input_.reset();

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
    loader.load(content, runSession_, resolvedRequest, level);

    initializeRuntimeInteraction(registry_);
    initializeRuntimeInventory(registry_);
    initializeRuntimeDoors(registry_);
    initializeRuntimeCheckpoints(registry_);
    physics_.update(registry_, 0.0f);
}

void RuntimeGameSession::clear() {
    clearEntities();
    input_.reset();
}

void RuntimeGameSession::tick(float deltaTime, float aspect) {
    if (!physicsInitialized_) {
        return;
    }
    updateRuntimeInteraction(registry_, input_);
    updateRuntimeDoors(registry_, deltaTime);
    updateRuntimeCheckpoints(registry_, deltaTime, runSession_);
    physics_.update(registry_, deltaTime);

    ContentRegistry* content = nullptr;
    if (registry_.ctx().contains<ContentRegistry*>()) {
        content = registry_.ctx().get<ContentRegistry*>();
    }
    if (content != nullptr) {
        updateRuntimeInventory(registry_, input_, runSession_, *content);
    }
    updateRuntimePlayerMovement(registry_, input_, physics_, deltaTime);
    updateRuntimeCamera(registry_, input_, aspect, deltaTime);
}

void RuntimeGameSession::prewarmRenderer(ContentRegistry& content) {
    content_ = &content;
    ensureRendererInitialized();
}

RuntimeSceneRenderOutput RuntimeGameSession::render(float deltaTime,
                                                    int internalWidth,
                                                    int internalHeight,
                                                    int outputWidth,
                                                    int outputHeight,
                                                    GLuint targetFramebuffer) {
    RuntimeSceneRenderOutput output;
    if (!physicsInitialized_) {
        return output;
    }
    ensureRendererInitialized();
    if (!rendererInitialized_) {
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
    return output;
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
}
