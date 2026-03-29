#include "WardenOfficeScene.h"

#include "engine/core/Application.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelLoader.h"
#include "game/levels/prison/PrisonAssets.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/EnvironmentProfile.h"

void WardenOfficeScene::onEnter(Application& app) {
    entities_.clear();
    LevelBuildContext context{
        .registry = app.registry(),
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    request_.registerAssets = [](MeshLibrary& library) {
        registerPrisonAssets(library);
    };
    request_.buildScriptedGeometry = {};
    LevelLoader loader(context);
    loader.load(app, request_);
}

void WardenOfficeScene::onExit(Application& app) {
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
