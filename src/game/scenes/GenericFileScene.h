#pragma once

#include "engine/scene/Scene.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/level/LevelLoader.h"

#include <entt/entt.hpp>
#include <string>
#include <vector>

/// Scene that loads any .scene file by path via LevelLoader.
/// Used by the runtime --scene argument (D-18) to open an arbitrary scene file.
class GenericFileScene : public Scene {
public:
    explicit GenericFileScene(const std::string& scenePath);

    void onEnter(Application& app) override;
    void onExit(Application& app) override;

private:
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    LevelLoadRequest request_;
};
