#include "game/levels/cathedral/CathedralPrefabs.h"

#include "game/levels/cathedral/CathedralBuilder.h"
#include "game/levels/cathedral/CathedralSceneData.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"

void spawnCathedralCheckpointShrineGeometry(CathedralBuilder& builder, float chamberCenterZ) {
    const glm::vec3 shrineBase(0.0f, -0.02f, chamberCenterZ - 1.7f);
    builder.addMesh("cube", shrineBase, glm::vec3(2.7f, 0.32f, 2.2f));
    builder.addMesh("cube", shrineBase + glm::vec3(0.0f, 0.32f, 0.0f), glm::vec3(2.1f, 0.12f, 1.7f));
    builder.addMesh("cube", glm::vec3(0.0f, 0.72f, chamberCenterZ - 1.7f), glm::vec3(0.45f, 1.1f, 0.45f));
    builder.addMesh("cube", glm::vec3(0.0f, 1.55f, chamberCenterZ - 1.7f), glm::vec3(0.8f, 0.12f, 0.8f));
    builder.addMesh("cylinder", glm::vec3(0.0f, 1.25f, chamberCenterZ - 1.7f), glm::vec3(0.26f, 0.5f, 0.26f));
}

void spawnCathedralSideShrineBay(CathedralBuilder& builder, float side, float halfW, float z) {
    const float wallX = side * (halfW - 0.25f);
    const float frameX = side * (halfW - 1.1f);

    builder.addMesh("cube", glm::vec3(wallX, 2.45f, z), glm::vec3(0.12f, 2.6f, 1.8f));
    builder.addMesh("cube", glm::vec3(frameX, 4.3f, z), glm::vec3(0.35f, 0.35f, 2.2f));
    builder.addMesh("cube", glm::vec3(frameX, 0.95f, z), glm::vec3(0.35f, 0.2f, 2.2f));
    builder.addMesh("cube", glm::vec3(frameX, 2.55f, z - 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));
    builder.addMesh("cube", glm::vec3(frameX, 2.55f, z + 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));

    for (float barOffset : {-0.7f, 0.0f, 0.7f}) {
        builder.addMesh("cube", glm::vec3(side * (halfW - 1.45f), 2.55f, z + barOffset), glm::vec3(0.1f, 1.9f, 0.08f));
    }

    builder.addMesh(
        "cube",
        glm::vec3(side * (halfW - 1.85f), 0.35f, z),
        glm::vec3(0.8f, 0.45f, 1.1f),
        glm::vec3(0.0f, 0.0f, side * 6.0f)
    );
}

void spawnCathedralDaisStep(CathedralBuilder& builder, const glm::vec3& center, const glm::vec3& scale) {
    builder.addMesh("cube", center, scale);
    builder.addBoxCollider(center, scale * 0.5f);
}

namespace {

entt::entity spawnDoorLeaf(CathedralBuilder& builder,
                           Mesh* mesh,
                           const glm::vec3& closedCenter,
                           const glm::vec3& hingePosition,
                           const glm::vec3& leafScale,
                           float closedYaw,
                           float openYaw) {
    auto leaf = builder.addMesh(mesh, closedCenter, leafScale);
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

entt::entity spawnCathedralCheckpoint(CathedralBuilder& builder, const CathedralCheckpointPlacement& placement) {
    auto checkpointLight = builder.addLight(
        placement.lightPosition,
        placement.lightColor,
        placement.lightRadius,
        placement.lightIntensity
    );

    auto checkpoint = builder.createTransformEntity(placement.position);
    builder.registry().emplace<CheckpointComponent>(
        checkpoint,
        CheckpointComponent{
            placement.respawnPosition,
            placement.interactDistance,
            placement.interactDotThreshold,
            false,
            checkpointLight
        }
    );
    return checkpoint;
}

entt::entity spawnCathedralDoubleDoor(CathedralBuilder& builder,
                                      Mesh* leftDoorMesh,
                                      Mesh* rightDoorMesh,
                                      const CathedralDoubleDoorPlacement& placement) {
    const glm::vec3 leftCenter = placement.leftHingePosition + glm::vec3(placement.leafScale.x * 0.5f, 0.0f, 0.0f);
    const glm::vec3 rightCenter = placement.rightHingePosition - glm::vec3(placement.leafScale.x * 0.5f, 0.0f, 0.0f);

    auto leftLeaf = spawnDoorLeaf(
        builder,
        leftDoorMesh,
        leftCenter,
        placement.leftHingePosition,
        placement.leafScale,
        placement.closedYaw,
        placement.closedYaw - placement.openAngle
    );
    auto rightLeaf = spawnDoorLeaf(
        builder,
        rightDoorMesh,
        rightCenter,
        placement.rightHingePosition,
        placement.leafScale,
        placement.closedYaw,
        placement.closedYaw + placement.openAngle
    );

    if (leftLeaf == entt::null || rightLeaf == entt::null) {
        return entt::null;
    }

    auto doorRoot = builder.createTransformEntity(placement.rootPosition);
    builder.registry().emplace<DoorComponent>(
        doorRoot,
        DoorComponent{
            leftLeaf,
            rightLeaf,
            placement.interactDistance,
            placement.interactDotThreshold,
            placement.openDuration,
            0.0f,
            false,
            false
        }
    );
    return doorRoot;
}
