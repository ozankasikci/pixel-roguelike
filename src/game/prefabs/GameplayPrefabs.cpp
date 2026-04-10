#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/modules/checkpoint/CheckpointSpawner.h"

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec) {
    return spawnCheckpointPrefab(builder, spec);
}

entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance) {
    switch (instance.type) {
    case GameplayPrefabType::Checkpoint:
        return spawnCheckpoint(builder, instance.checkpoint);
    }
    return entt::null;
}
