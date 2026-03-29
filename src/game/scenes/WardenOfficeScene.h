#pragma once

#include "engine/scene/Scene.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/level/LevelLoader.h"

#include <entt/entt.hpp>
#include <vector>

class WardenOfficeScene : public Scene {
public:
    void onEnter(Application& app) override;
    void onExit(Application& app) override;

private:
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    LevelLoadRequest request_{
        .levelId = "warden_office",
        .levelPath = "assets/scenes/warden_office.scene",
    };
};
