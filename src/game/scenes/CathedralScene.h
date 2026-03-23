#pragma once
#include "engine/scene/Scene.h"
#include "engine/rendering/MeshLibrary.h"
#include <entt/entt.hpp>
#include <vector>

class CathedralScene : public Scene {
public:
    void onEnter(Application& app) override;
    void onExit(Application& app) override;

private:
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
};
