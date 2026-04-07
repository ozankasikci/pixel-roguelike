#pragma once

#include "game/prefabs/GameplayPrefabData.h"

#include <entt/entt.hpp>

class LevelBuilder;

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec);
entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance);
