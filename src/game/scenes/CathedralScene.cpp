#include "CathedralScene.h"

#include "engine/core/Application.h"
#include "game/level/LevelBuildContext.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/levels/cathedral/CathedralLayout.h"

void CathedralScene::onEnter(Application& app) {
    entities_.clear();
    registerCathedralAssets(meshLibrary_);

    LevelBuildContext context{
        .registry = app.registry(),
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    buildCathedralLayout(context);
}

void CathedralScene::onExit(Application& app) {
    for (auto e : entities_) {
        if (app.registry().valid(e)) {
            app.registry().destroy(e);
        }
    }
    entities_.clear();
    meshLibrary_.clear();
}
