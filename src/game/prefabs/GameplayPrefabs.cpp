#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec) {
    auto checkpointLight = builder.addLight(
        spec.lightPosition,
        spec.lightColor,
        spec.lightRadius,
        spec.lightIntensity
    );

    auto checkpoint = builder.createTransformEntity(spec.position);
    builder.registry().emplace<CheckpointComponent>(
        checkpoint,
        CheckpointComponent{
            spec.respawnPosition,
            spec.interactDistance,
            spec.interactDotThreshold,
            false,
            checkpointLight
        }
    );
    builder.registry().emplace<InteractableComponent>(
        checkpoint,
        InteractableComponent{
            "E  KINDLE CHECKPOINT",
            "CHECKPOINT KINDLED",
            spec.interactDistance,
            spec.interactDotThreshold,
            true,
            false
        }
    );
    return checkpoint;
}

entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance) {
    switch (instance.type) {
    case GameplayPrefabType::Checkpoint:
        return spawnCheckpoint(builder, instance.checkpoint);
    }
    return entt::null;
}
