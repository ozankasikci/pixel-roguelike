#pragma once

#include "engine/rendering/MeshLibrary.h"

#include <entt/entt.hpp>
#include <vector>

struct CathedralContext {
    entt::registry& registry;
    MeshLibrary& meshLibrary;
    std::vector<entt::entity>& entities;
};
