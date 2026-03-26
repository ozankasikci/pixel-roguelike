#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/StaticColliderComponent.h"

namespace {

entt::entity spawnDoorLeaf(LevelBuilder& builder,
                           Mesh* mesh,
                           const glm::vec3& closedCenter,
                           const glm::vec3& hingePosition,
                           const glm::vec3& leafScale,
                           float closedYaw,
                           float openYaw,
                           const glm::vec3& tint) {
    auto leaf = builder.addMesh(
        mesh,
        closedCenter,
        leafScale,
        glm::vec3(0.0f),
        tint,
        MaterialKind::Wood
    );
    if (leaf == entt::null) {
        return entt::null;
    }

    auto& registry = builder.registry();

    StaticColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.position = closedCenter;
    collider.rotation = glm::vec3(0.0f, closedYaw, 0.0f);
    collider.halfExtents = leafScale * 0.5f;
    registry.emplace<StaticColliderComponent>(leaf, collider);

    DoorLeafComponent doorLeaf;
    doorLeaf.hingePosition = hingePosition;
    doorLeaf.centerOffsetFromHinge = closedCenter - hingePosition;
    doorLeaf.closedScale = leafScale;
    doorLeaf.colliderHalfExtents = collider.halfExtents;
    doorLeaf.closedYaw = closedYaw;
    doorLeaf.openYaw = openYaw;
    registry.emplace<DoorLeafComponent>(leaf, doorLeaf);
    return leaf;
}

} // namespace

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

entt::entity spawnDoubleDoor(LevelBuilder& builder,
                             Mesh* leftDoorMesh,
                             Mesh* rightDoorMesh,
                             const DoubleDoorSpawnSpec& spec) {
    const glm::vec3 leftCenter = spec.leftHingePosition + glm::vec3(spec.leafScale.x * 0.5f, 0.0f, 0.0f);
    const glm::vec3 rightCenter = spec.rightHingePosition - glm::vec3(spec.leafScale.x * 0.5f, 0.0f, 0.0f);

    auto leftLeaf = spawnDoorLeaf(
        builder,
        leftDoorMesh,
        leftCenter,
        spec.leftHingePosition,
        spec.leafScale,
        spec.closedYaw,
        spec.closedYaw - spec.openAngle,
        RetroPalette::OldWood
    );
    auto rightLeaf = spawnDoorLeaf(
        builder,
        rightDoorMesh,
        rightCenter,
        spec.rightHingePosition,
        spec.leafScale,
        spec.closedYaw,
        spec.closedYaw + spec.openAngle,
        RetroPalette::OldWoodDark
    );

    if (leftLeaf == entt::null || rightLeaf == entt::null) {
        return entt::null;
    }

    auto doorRoot = builder.createTransformEntity(spec.rootPosition);
    builder.registry().emplace<DoorComponent>(
        doorRoot,
        DoorComponent{
            leftLeaf,
            rightLeaf,
            spec.interactDistance,
            spec.interactDotThreshold,
            spec.openDuration,
            0.0f,
            false,
            false
        }
    );
    builder.registry().emplace<InteractableComponent>(
        doorRoot,
        InteractableComponent{
            "E  OPEN HEAVY DOOR",
            "OPENING HEAVY DOOR",
            spec.interactDistance,
            spec.interactDotThreshold,
            true,
            false
        }
    );
    return doorRoot;
}

entt::entity spawnDoubleDoor(LevelBuilder& builder, const DoubleDoorSpawnSpec& spec) {
    return spawnDoubleDoor(
        builder,
        builder.mesh(spec.leftLeafMeshName),
        builder.mesh(spec.rightLeafMeshName),
        spec
    );
}

entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance) {
    switch (instance.type) {
    case GameplayPrefabType::Checkpoint:
        return spawnCheckpoint(builder, instance.checkpoint);
    case GameplayPrefabType::DoubleDoor:
        return spawnDoubleDoor(builder, instance.doubleDoor);
    }

    return entt::null;
}
