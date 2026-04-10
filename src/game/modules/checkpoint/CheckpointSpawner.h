#pragma once

#include <entt/entity/fwd.hpp>

struct CheckpointSpawnSpec;
struct LevelCheckpointPlacement;
class LevelBuilder;

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement);

entt::entity spawnCheckpointPrefab(LevelBuilder& builder,
                                   const CheckpointSpawnSpec& spec);
