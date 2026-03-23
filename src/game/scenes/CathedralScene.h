#pragma once
#include "engine/scene/Scene.h"
#include "engine/rendering/Mesh.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

class CathedralScene : public Scene {
public:
    void onEnter(Application& app) override;
    void onExit(Application& app) override;

private:
    // Mesh storage -- CathedralScene owns mesh memory, entities reference via pointer
    std::vector<std::unique_ptr<Mesh>> meshes_;
    // Track spawned entities for cleanup in onExit
    std::vector<entt::entity> entities_;
};
