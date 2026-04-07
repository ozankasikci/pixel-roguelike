#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorConfigComponent.h"
#include "game/components/DoorStateComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/ColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    entt::registry registry;
    MeshLibrary meshLibrary;
    std::vector<entt::entity> entities;
    LevelBuildContext context{registry, meshLibrary, entities};
    LevelBuilder builder(context);

    const CheckpointSpawnSpec checkpointPlacement{
        glm::vec3(0.0f, 1.3f, -35.3f),
        glm::vec3(0.0f, 1.6f, -32.8f),
        2.4f,
        0.55f,
        glm::vec3(0.0f, 2.6f, -35.3f),
        glm::vec3(1.0f, 0.82f, 0.62f),
        5.5f,
        4.0f
    };

    const entt::entity checkpoint = spawnCheckpoint(builder, checkpointPlacement);
    assert(registry.valid(checkpoint));
    assert(entities.size() == 2);
    assert((registry.all_of<TransformComponent, CheckpointComponent>(checkpoint)));
    assert(registry.get<TransformComponent>(checkpoint).position == checkpointPlacement.position);

    const auto& checkpointComponent = registry.get<CheckpointComponent>(checkpoint);
    const auto& checkpointInteractable = registry.get<InteractableComponent>(checkpoint);
    assert(checkpointComponent.respawnPosition == checkpointPlacement.respawnPosition);
    assert(checkpointComponent.interactDistance == checkpointPlacement.interactDistance);
    assert(checkpointComponent.interactDotThreshold == checkpointPlacement.interactDotThreshold);
    assert(checkpointInteractable.promptText == "E  KINDLE CHECKPOINT");
    assert(registry.valid(checkpointComponent.lightEntity));
    assert((registry.all_of<TransformComponent, LightComponent>(checkpointComponent.lightEntity)));
    assert(registry.get<TransformComponent>(checkpointComponent.lightEntity).position == checkpointPlacement.lightPosition);

    const auto* leftDoorMesh = reinterpret_cast<Mesh*>(static_cast<uintptr_t>(0x1));
    const auto* rightDoorMesh = reinterpret_cast<Mesh*>(static_cast<uintptr_t>(0x2));
    DoubleDoorSpawnSpec doorPlacement;
    doorPlacement.rootPosition = glm::vec3(0.0f, 3.13f, -19.4f);
    doorPlacement.leftHingePosition = glm::vec3(-2.315f, 3.13f, -19.4f);
    doorPlacement.rightHingePosition = glm::vec3(2.315f, 3.13f, -19.4f);
    doorPlacement.leafScale = glm::vec3(2.315f, 6.26f, 0.30f);
    doorPlacement.closedYaw = 0.0f;
    doorPlacement.openAngle = 92.0f;
    doorPlacement.interactDistance = 3.2f;
    doorPlacement.interactDotThreshold = 0.72f;
    doorPlacement.openDuration = 2.4f;

    const entt::entity doorRoot = spawnDoubleDoor(
        builder,
        const_cast<Mesh*>(leftDoorMesh),
        const_cast<Mesh*>(rightDoorMesh),
        doorPlacement
    );
    assert(registry.valid(doorRoot));
    assert(entities.size() == 5);
    assert((registry.all_of<TransformComponent, DoorConfigComponent, DoorStateComponent>(doorRoot)));
    assert(registry.get<TransformComponent>(doorRoot).position == doorPlacement.rootPosition);

    const auto& doorConfig = registry.get<DoorConfigComponent>(doorRoot);
    const auto& doorInteractable = registry.get<InteractableComponent>(doorRoot);
    assert(registry.valid(doorConfig.leftLeaf));
    assert(registry.valid(doorConfig.rightLeaf));
    assert(doorConfig.interactDistance == doorPlacement.interactDistance);
    assert(doorConfig.interactDotThreshold == doorPlacement.interactDotThreshold);
    assert(doorConfig.openDuration == doorPlacement.openDuration);
    assert(doorInteractable.promptText == "E  OPEN HEAVY DOOR");

    // spawnDoorLeaf sets basePosition = hingePosition, pivot = closedCenter - hingePosition
    const glm::vec3 expectedLeftCenter  = doorPlacement.leftHingePosition  + glm::vec3(doorPlacement.leafScale.x * 0.5f, 0.0f, 0.0f);
    const glm::vec3 expectedRightCenter = doorPlacement.rightHingePosition - glm::vec3(doorPlacement.leafScale.x * 0.5f, 0.0f, 0.0f);
    const glm::vec3 expectedLeftPivot   = expectedLeftCenter  - doorPlacement.leftHingePosition;
    const glm::vec3 expectedRightPivot  = expectedRightCenter - doorPlacement.rightHingePosition;

    const auto& leftLeaf = registry.get<DoorLeafComponent>(doorConfig.leftLeaf);
    const auto& leftCollider = registry.get<ColliderComponent>(doorConfig.leftLeaf);
    const auto& leftMesh = registry.get<MeshComponent>(doorConfig.leftLeaf);
    assert(leftLeaf.basePosition == doorPlacement.leftHingePosition);
    assert(leftLeaf.pivot == expectedLeftPivot);
    assert(leftLeaf.closedScale == doorPlacement.leafScale);
    assert(leftLeaf.closedYaw == doorPlacement.closedYaw);
    assert(leftLeaf.openYaw == doorPlacement.closedYaw - doorPlacement.openAngle);
    assert(leftCollider.position == expectedLeftCenter);
    assert(leftCollider.halfExtents == doorPlacement.leafScale * 0.5f);
    assert(leftMesh.mesh == leftDoorMesh);

    const auto& rightLeaf = registry.get<DoorLeafComponent>(doorConfig.rightLeaf);
    const auto& rightCollider = registry.get<ColliderComponent>(doorConfig.rightLeaf);
    const auto& rightMesh = registry.get<MeshComponent>(doorConfig.rightLeaf);
    assert(rightLeaf.basePosition == doorPlacement.rightHingePosition);
    assert(rightLeaf.pivot == expectedRightPivot);
    assert(rightLeaf.openYaw == doorPlacement.closedYaw + doorPlacement.openAngle);
    assert(rightCollider.position == expectedRightCenter);
    assert(rightMesh.mesh == rightDoorMesh);

    return 0;
}
