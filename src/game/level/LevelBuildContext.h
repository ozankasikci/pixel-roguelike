#pragma once

#include "engine/rendering/geometry/MeshLibrary.h"

#include <entt/entt.hpp>
#include <vector>

struct LevelBuildContext {
    entt::registry& registry;
    MeshLibrary& meshLibrary;
    std::vector<entt::entity>& entities;
};
