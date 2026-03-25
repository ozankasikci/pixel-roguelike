#pragma once

#include "game/prefabs/GameplayPrefabData.h"

#include <string>

struct GameplayPrefabAsset {
    GameplayPrefabType type = GameplayPrefabType::Checkpoint;
    CheckpointSpawnSpec checkpoint;
    DoubleDoorSpawnSpec doubleDoor;
};

GameplayPrefabAsset loadGameplayPrefabAsset(const std::string& path);
GameplayPrefabInstance instantiateGameplayPrefab(const GameplayPrefabAsset& asset,
                                                 const glm::vec3& position,
                                                 float yawDegrees = 0.0f);
