#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/StaticColliderComponent.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr glm::vec3 kDoorRomanesqueLeft(0.33f, 0.15f, 0.09f);
constexpr glm::vec3 kDoorRomanesqueRight(0.26f, 0.11f, 0.07f);

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
        std::string("wood_default")
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
        kDoorRomanesqueLeft
    );
    auto rightLeaf = spawnDoorLeaf(
        builder,
        rightDoorMesh,
        rightCenter,
        spec.rightHingePosition,
        spec.leafScale,
        spec.closedYaw,
        spec.closedYaw + spec.openAngle,
        kDoorRomanesqueRight
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

entt::entity spawnSingleDoor(LevelBuilder& builder, const SingleDoorSpawnSpec& spec) {
    // Place the door frame (static visual mesh)
    builder.addMesh(spec.frameMeshName,
        spec.rootPosition,
        glm::vec3(0.21f),
        glm::vec3(0.0f, spec.doorYawDegrees, 0.0f),
        spec.frameTint,
        spec.frameMaterialId);

    // Compute hinge world position by rotating local hinge offset by doorYawDegrees
    // Local hinge offset: left jamb at roughly (-0.45, 0, 0.04) in door-facing space
    const float yawRad = glm::radians(spec.doorYawDegrees);
    const glm::vec3 localHingeOffset(-0.45f, 0.0f, 0.04f);
    const glm::vec3 hingeWorldPos = spec.rootPosition + glm::vec3(
        localHingeOffset.x * std::cos(yawRad) - localHingeOffset.z * std::sin(yawRad),
        0.0f,
        localHingeOffset.x * std::sin(yawRad) + localHingeOffset.z * std::cos(yawRad)
    );

    // Center of door leaf is ~0.42m in the +X direction from hinge (in hinge-local space)
    // This is the offset used for rotation animation
    const glm::vec3 centerOffsetFromHinge(0.42f, 0.0f, 0.0f);

    // Compute initial world position of door leaf center (for StaticCollider)
    const glm::vec3 leafWorldCenter = hingeWorldPos + glm::vec3(
        centerOffsetFromHinge.x * std::cos(yawRad),
        0.0f,
        centerOffsetFromHinge.x * std::sin(yawRad)
    );

    // Create the door leaf mesh entity at hinge position (RuntimeGameplay uses hinge + offset)
    Mesh* leafMesh = builder.mesh(spec.doorMeshName);
    auto leaf = builder.addMesh(leafMesh,
        hingeWorldPos,
        glm::vec3(0.21f),
        glm::vec3(0.0f, spec.doorYawDegrees, 0.0f),
        spec.doorTint,
        spec.doorMaterialId);

    if (leaf == entt::null) {
        return entt::null;
    }

    auto& registry = builder.registry();

    // Attach DoorLeafComponent for swing animation
    DoorLeafComponent doorLeaf;
    doorLeaf.hingePosition = hingeWorldPos;
    doorLeaf.centerOffsetFromHinge = centerOffsetFromHinge;
    doorLeaf.closedScale = glm::vec3(0.21f);
    doorLeaf.colliderHalfExtents = glm::vec3(0.42f, 0.96f, 0.05f);
    doorLeaf.closedYaw = spec.doorYawDegrees;
    doorLeaf.openYaw = spec.doorYawDegrees - spec.openAngle;
    registry.emplace<DoorLeafComponent>(leaf, doorLeaf);

    // Attach StaticColliderComponent for physics blocking
    StaticColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.position = leafWorldCenter;
    collider.rotation = glm::vec3(0.0f, spec.doorYawDegrees, 0.0f);
    collider.halfExtents = glm::vec3(0.42f, 0.96f, 0.05f);
    registry.emplace<StaticColliderComponent>(leaf, collider);

    // Create the door root entity (interaction trigger point at handle height)
    const glm::vec3 rootPos = spec.rootPosition + glm::vec3(0.0f, 1.0f, 0.0f);
    auto doorRoot = builder.createTransformEntity(rootPos);

    if (spec.locked) {
        // Locked door: only InteractableComponent, no DoorComponent
        registry.emplace<InteractableComponent>(doorRoot,
            InteractableComponent{
                spec.lockedPrompt,
                "",
                spec.interactDistance,
                spec.interactDotThreshold,
                true,
                false
            });
    } else {
        // Openable door
        registry.emplace<DoorComponent>(doorRoot,
            DoorComponent{
                leaf,
                entt::null,
                spec.interactDistance,
                spec.interactDotThreshold,
                spec.openDuration,
                0.0f,
                false,
                false
            });
        registry.emplace<InteractableComponent>(doorRoot,
            InteractableComponent{
                "E  OPEN DOOR",
                "OPENING DOOR",
                spec.interactDistance,
                spec.interactDotThreshold,
                true,
                false
            });
    }

    return doorRoot;
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
