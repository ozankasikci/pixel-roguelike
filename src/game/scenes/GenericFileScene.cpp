#include "GenericFileScene.h"

#include "engine/core/Application.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelLoader.h"
#include "game/levels/ProceduralGameAssets.h"
#include "game/prefabs/GameplayPrefabs.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/session/RunSession.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"

#include <filesystem>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Scripted geometry registry
// ---------------------------------------------------------------------------

using ScriptedGeometryCallback = std::function<void(LevelBuilder&)>;

static std::unordered_map<std::string, ScriptedGeometryCallback>& scriptedGeometryRegistry() {
    static std::unordered_map<std::string, ScriptedGeometryCallback> registry;
    return registry;
}

// ---------------------------------------------------------------------------
// Built-in scripted geometry implementations
// ---------------------------------------------------------------------------

static void buildInstitutionalRoomGeometry(LevelBuilder& builder) {
    // Metal door (center) - locked stub per D-04
    auto metalDoor = builder.addMesh("wood_door",
        glm::vec3(0.0f, 0.0f, 5.95f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.60f, 0.58f, 0.55f),
        std::string("wood_door_1"));
    builder.registry().emplace<InteractableComponent>(metalDoor,
        InteractableComponent{
            .promptText = "E  This door is locked",
            .interactDistance = 2.0f,
            .enabled = true
        });
    // Knob on metal door (handle height ~1.0m, on front face)
    builder.addMesh("inst_door_knob",
        glm::vec3(0.15f, 1.0f, 5.90f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.50f, 0.48f, 0.45f),
        std::string("metal_default"));

    // Chained door (right) - locked stub per D-04
    auto chainedDoor = builder.addMesh("wood_door",
        glm::vec3(2.5f, 0.0f, 5.95f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.88f, 0.86f, 0.82f),
        std::string("wood_door_1"));
    builder.registry().emplace<InteractableComponent>(chainedDoor,
        InteractableComponent{
            .promptText = "E  This door is locked",
            .interactDistance = 2.0f,
            .enabled = true
        });
    // Knob on chained door
    builder.addMesh("inst_door_knob",
        glm::vec3(2.65f, 1.0f, 5.90f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.50f, 0.48f, 0.45f),
        std::string("metal_default"));
}

static void buildInitialSceneGeometry(LevelBuilder& builder) {
    // Door A: Staff Break Room (West wall at X=-5, Z=1) — openable
    spawnSingleDoor(builder, SingleDoorSpawnSpec{
        .doorMeshName = "SM_DoorA",
        .frameMeshName = "SM_FrameA",
        .doorMaterialId = "qdp_door_a",
        .frameMaterialId = "qdp_door_a",
        .rootPosition = glm::vec3(-5.0f, 0.0f, 1.0f),
        .doorYawDegrees = 90.0f,
        .openAngle = 80.0f,
        .openDuration = 1.2f,
        .locked = false,
        .doorTint = glm::vec3(1.0f),
        .frameTint = glm::vec3(1.0f),
    });

    // Door B: Maintenance Corridor (North wall at Z=6, center X=0) — openable
    spawnSingleDoor(builder, SingleDoorSpawnSpec{
        .doorMeshName = "SM_DoorD",
        .frameMeshName = "SM_FrameD",
        .doorMaterialId = "qdp_door_d",
        .frameMaterialId = "qdp_door_d",
        .rootPosition = glm::vec3(0.0f, 0.0f, 6.0f),
        .doorYawDegrees = 180.0f,
        .openAngle = 85.0f,
        .openDuration = 1.5f,
        .locked = false,
        .doorTint = glm::vec3(0.60f, 0.58f, 0.55f),
        .frameTint = glm::vec3(0.65f, 0.63f, 0.60f),
    });

    // Door C: Observation Room (East wall at X=5, Z=3) — locked with chain/padlock
    spawnSingleDoor(builder, SingleDoorSpawnSpec{
        .doorMeshName = "SM_DoorC",
        .frameMeshName = "SM_FrameC",
        .doorMaterialId = "qdp_door_c",
        .frameMaterialId = "qdp_door_c",
        .rootPosition = glm::vec3(5.0f, 0.0f, 3.0f),
        .doorYawDegrees = -90.0f,
        .locked = true,
        .lockedPrompt = "E  This door is chained shut",
        .doorTint = glm::vec3(0.88f, 0.85f, 0.80f),
        .frameTint = glm::vec3(0.85f, 0.82f, 0.78f),
    });

    // Chain and padlock on Door C (east wall, near door opening)
    builder.addMesh("inst_chain_padlock",
        glm::vec3(4.90f, 1.1f, 3.0f),
        glm::vec3(1.0f),
        glm::vec3(0.0f, -90.0f, 0.0f),
        glm::vec3(0.20f, 0.16f, 0.12f),
        std::string("metal_default"));
}

// ---------------------------------------------------------------------------
// Register built-in scripted geometry at static init time
// ---------------------------------------------------------------------------

namespace {
const bool kBuiltinsRegistered = [] {
    GenericFileScene::registerScriptedGeometry("institutional_room",
        buildInstitutionalRoomGeometry);
    GenericFileScene::registerScriptedGeometry("initial_scene",
        buildInitialSceneGeometry);
    return true;
}();
} // namespace

// ---------------------------------------------------------------------------
// GenericFileScene implementation
// ---------------------------------------------------------------------------

void GenericFileScene::registerScriptedGeometry(const std::string& levelId,
                                                 std::function<void(LevelBuilder&)> callback) {
    scriptedGeometryRegistry()[levelId] = std::move(callback);
}

GenericFileScene::GenericFileScene(const std::string& scenePath) {
    request_.levelId   = std::filesystem::path(scenePath).stem().string();
    request_.levelPath = scenePath;
}

void GenericFileScene::onEnter(Application& app) {
    entities_.clear();
    LevelBuildContext context{
        .registry    = app.registry(),
        .meshLibrary = meshLibrary_,
        .entities    = entities_,
    };
    request_.registerAssets = [](MeshLibrary& library) {
        registerProceduralAssets(library);

        // Auto-discover file-based meshes from assets/meshes/ (per D-05)
        const std::filesystem::path meshDir(resolveProjectPath("assets/meshes"));
        for (const auto& asset : ModelLoader::discoverProjectAssets(meshDir, std::filesystem::current_path())) {
            if (!library.has(asset.meshId)) {
                library.registerFileAlias(asset.meshId, asset.relativePath);
            }
        }

        // Auto-discover file-based meshes from assets/packs/ subdirectories
        const std::filesystem::path packsDir(resolveProjectPath("assets/packs"));
        for (const auto& asset : ModelLoader::discoverProjectAssets(packsDir, std::filesystem::current_path())) {
            if (!library.has(asset.meshId)) {
                library.registerFileAlias(asset.meshId, asset.relativePath);
            }
        }
    };

    // Look up scripted geometry from the registry (no hard-coded if-chain)
    auto it = scriptedGeometryRegistry().find(request_.levelId);
    if (it != scriptedGeometryRegistry().end()) {
        request_.buildScriptedGeometry = it->second;
    } else {
        request_.buildScriptedGeometry = {};
    }

    LevelLoadArgs args{
        .content = &app.getService<ContentRegistry>(),
        .session = &app.getService<RunSession>()
    };
    LevelLoader loader(context);
    loader.load(request_, args);
}

void GenericFileScene::onExit(Application& app) {
    for (auto entity : entities_) {
        if (app.registry().valid(entity)) {
            app.registry().destroy(entity);
        }
    }
    entities_.clear();
    auto& ctx = app.registry().ctx();
    if (ctx.contains<MeshAssetProvider>()) {
        ctx.erase<MeshAssetProvider>();
    }
    if (ctx.contains<ActiveEnvironmentProfile>()) {
        ctx.erase<ActiveEnvironmentProfile>();
    }
    meshLibrary_.clear();
}
