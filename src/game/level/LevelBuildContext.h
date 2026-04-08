#pragma once

#include "engine/ecs/GameRegistry.h"
#include "engine/rendering/geometry/MeshLibrary.h"

#include <entt/entt.hpp>
#include <vector>

struct LevelBuildContext {
    GameRegistry& registry;
    MeshLibrary& meshLibrary;
    std::vector<entt::entity>& entities;
};
