#include "GenericFileScene.h"

#include "engine/core/Application.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelLoader.h"
#include "game/levels/ProceduralGameAssets.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/session/RunSession.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"

#include <filesystem>

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
