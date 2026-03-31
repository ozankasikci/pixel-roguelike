#include "GenericFileScene.h"

#include "engine/core/Application.h"
#include "game/components/InteractableComponent.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelLoader.h"
#include "game/levels/GameAssets.h"
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
        registerAllGameAssets(library);
    };
    if (request_.levelId == "institutional_room") {
        request_.buildScriptedGeometry = [](LevelBuilder& builder) {
            // Metal door (center) - locked stub per D-04
            auto metalDoor = builder.addMesh("office_door",
                glm::vec3(0.0f, 0.0f, 5.95f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(0.0f, 180.0f, 0.0f),
                glm::vec3(0.60f, 0.58f, 0.55f),
                std::string("metal_default"));
            builder.registry().emplace<InteractableComponent>(metalDoor,
                InteractableComponent{
                    .promptText = "E  This door is locked",
                    .interactDistance = 2.0f,
                    .enabled = true
                });

            // Chained door (right) - locked stub per D-04
            auto chainedDoor = builder.addMesh("office_door",
                glm::vec3(2.5f, 0.0f, 5.95f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(0.0f, 180.0f, 0.0f),
                glm::vec3(0.88f, 0.86f, 0.82f),
                std::string("stone_default"));
            builder.registry().emplace<InteractableComponent>(chainedDoor,
                InteractableComponent{
                    .promptText = "E  This door is locked",
                    .interactDistance = 2.0f,
                    .enabled = true
                });
        };
    } else {
        request_.buildScriptedGeometry = {};
    }
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
