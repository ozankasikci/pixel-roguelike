#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/ColliderComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

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

    ColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.mode = ColliderMode::Solid;
    collider.position = closedCenter;
    collider.rotation = glm::vec3(0.0f, closedYaw, 0.0f);
    collider.halfExtents = leafScale * 0.5f;
    registry.emplace<ColliderComponent>(leaf, collider);

    DoorLeafComponent doorLeaf;
    doorLeaf.basePosition = closedCenter;
    doorLeaf.pivot = glm::vec3(0.0f);  // legacy double-door path: no pivot offset
    doorLeaf.closedScale = leafScale;
    doorLeaf.colliderHalfExtents = collider.halfExtents;
    doorLeaf.closedYaw = closedYaw;
    doorLeaf.openYaw = openYaw;
    registry.emplace<DoorLeafComponent>(leaf, doorLeaf);
    return leaf;
}

} // namespace

glm::mat4 makePivotLeafModel(const glm::vec3& basePos,
                              float closedYawDeg,
                              float currentYawDeg,
                              const glm::vec3& pivot,
                              const glm::vec3& meshCenter,
                              const glm::vec3& scale) {
    // Door meshes have their origin at the hinge edge, not at the geometric center.
    // Frame meshes are centered at origin. To align the door within the frame:
    //   T(-meshCenter) shifts the door so its AABB center is at the origin.
    //
    // When closed (deltaYaw=0), the door center aligns with the frame center:
    //   T(basePos) * R(closedYaw) * S(scale) * T(-meshCenter)
    // When opening, the door rotates around the pivot (hinge) in mesh-local space:
    //   T(basePos) * R(closedYaw) * S * T(-meshCenter) * T(pivot) * R(deltaYaw) * T(-pivot)
    const float deltaYaw = currentYawDeg - closedYawDeg;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), basePos);
    model = glm::rotate(model, glm::radians(closedYawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    model = glm::translate(model, -meshCenter);
    model = glm::translate(model, pivot);
    model = glm::rotate(model, glm::radians(deltaYaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, -pivot);
    return model;
}

glm::vec3 computeHingeWorldPos(const glm::vec3& basePos,
                                float /*yawDeg*/,
                                const glm::vec3& /*pivot*/,
                                const glm::vec3& /*scale*/) {
    // The hinge IS at basePos — no offset needed
    return basePos;
}

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

entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance) {
    switch (instance.type) {
    case GameplayPrefabType::Checkpoint:
        return spawnCheckpoint(builder, instance.checkpoint);
    case GameplayPrefabType::DoubleDoor:
        return spawnDoubleDoor(builder, instance.doubleDoor);
    }

    return entt::null;
}
