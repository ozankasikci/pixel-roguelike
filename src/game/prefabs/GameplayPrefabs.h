#pragma once

#include "game/prefabs/GameplayPrefabData.h"

#include <entt/entt.hpp>

class LevelBuilder;
class Mesh;

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec);
entt::entity spawnDoubleDoor(LevelBuilder& builder,
                             Mesh* leftDoorMesh,
                             Mesh* rightDoorMesh,
                             const DoubleDoorSpawnSpec& spec);
entt::entity spawnDoubleDoor(LevelBuilder& builder, const DoubleDoorSpawnSpec& spec);
entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance);
