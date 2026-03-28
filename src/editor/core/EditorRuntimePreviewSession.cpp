#include "editor/core/EditorRuntimePreviewSession.h"

#include "engine/core/PathUtils.h"
#include "editor/EditorSceneDocument.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelLoader.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/session/RunSession.h"

#include <filesystem>

namespace {

void bootstrapPreviewMeshLibrary(MeshLibrary& meshLibrary) {
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
                meshLibrary.loadFromFile(meshId, std::filesystem::relative(entry.path(), std::filesystem::current_path()).generic_string());
            }
        }
    }

    const std::string staticDoorPath = resolveProjectPath("assets/meshes/gothic_door_static.glb");
    if (std::filesystem::exists(staticDoorPath) && !meshLibrary.has("gothic_door_static")) {
        meshLibrary.loadFromFile("gothic_door_static", "assets/meshes/gothic_door_static.glb");
    }
}

} // namespace

EditorRuntimePreviewSession::EditorRuntimePreviewSession() {
    bootstrapPreviewMeshLibrary(meshLibrary_);
}

void EditorRuntimePreviewSession::rebuild(const EditorSceneDocument& document, ContentRegistry& content) {
    clearEntities();
    session_ = RunSession{};

    registry_.ctx().insert_or_assign<ContentRegistry*>(&content);
    registry_.ctx().insert_or_assign(RuntimeEnvironmentOverride{document.environment()});

    LevelBuildContext context{
        .registry = registry_,
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    LevelLoader loader(context);
    LevelLoadRequest request;
    request.levelId = document.scenePath().empty() ? "editor_runtime_preview" : document.scenePath();
    request.levelPath = document.scenePath();
    request.registerAssets = [](MeshLibrary& library) {
        bootstrapPreviewMeshLibrary(library);
    };
    loader.load(content, session_, request, document.toLevelDef());
}

void EditorRuntimePreviewSession::clear() {
    clearEntities();
}

void EditorRuntimePreviewSession::clearEntities() {
    for (auto entity : entities_) {
        if (registry_.valid(entity)) {
            registry_.destroy(entity);
        }
    }
    entities_.clear();

    auto& ctx = registry_.ctx();
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
}
