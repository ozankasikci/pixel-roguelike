#pragma once

#include <entt/entt.hpp>

class LevelBuilder;
struct LevelCheckpointPlacement;

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement);
