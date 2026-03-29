#include "GenericFileScene.h"

#include "engine/core/Application.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelLoader.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/EnvironmentProfile.h"

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
        registerCathedralAssets(library);
    };
    request_.buildScriptedGeometry = {};
    LevelLoader loader(context);
    loader.load(app, request_);
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
