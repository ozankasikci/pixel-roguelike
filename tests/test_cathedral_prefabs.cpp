#include "engine/rendering/MeshLibrary.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"
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
    assert(checkpointComponent.respawnPosition == checkpointPlacement.respawnPosition);
    assert(checkpointComponent.interactDistance == checkpointPlacement.interactDistance);
    assert(checkpointComponent.interactDotThreshold == checkpointPlacement.interactDotThreshold);
    assert(registry.valid(checkpointComponent.lightEntity));
    assert((registry.all_of<TransformComponent, LightComponent>(checkpointComponent.lightEntity)));
    assert(registry.get<TransformComponent>(checkpointComponent.lightEntity).position == checkpointPlacement.lightPosition);

    const auto* leftDoorMesh = reinterpret_cast<Mesh*>(static_cast<uintptr_t>(0x1));
    const auto* rightDoorMesh = reinterpret_cast<Mesh*>(static_cast<uintptr_t>(0x2));
    const DoubleDoorSpawnSpec doorPlacement{
        glm::vec3(0.0f, 3.13f, -19.4f),
        glm::vec3(-2.315f, 3.13f, -19.4f),
        glm::vec3(2.315f, 3.13f, -19.4f),
        glm::vec3(2.315f, 6.26f, 0.30f),
        0.0f,
        92.0f,
        3.2f,
        0.72f,
        2.4f
    };

    const entt::entity doorRoot = spawnDoubleDoor(
        builder,
        const_cast<Mesh*>(leftDoorMesh),
        const_cast<Mesh*>(rightDoorMesh),
        doorPlacement
    );
    assert(registry.valid(doorRoot));
    assert(entities.size() == 5);
    assert((registry.all_of<TransformComponent, DoorComponent>(doorRoot)));
    assert(registry.get<TransformComponent>(doorRoot).position == doorPlacement.rootPosition);

    const auto& door = registry.get<DoorComponent>(doorRoot);
    assert(registry.valid(door.leftLeaf));
    assert(registry.valid(door.rightLeaf));
    assert(door.interactDistance == doorPlacement.interactDistance);
    assert(door.interactDotThreshold == doorPlacement.interactDotThreshold);
    assert(door.openDuration == doorPlacement.openDuration);

    const auto& leftLeaf = registry.get<DoorLeafComponent>(door.leftLeaf);
    const auto& leftCollider = registry.get<StaticColliderComponent>(door.leftLeaf);
    const auto& leftMesh = registry.get<MeshComponent>(door.leftLeaf);
    assert(leftLeaf.hingePosition == doorPlacement.leftHingePosition);
    assert(leftLeaf.closedScale == doorPlacement.leafScale);
    assert(leftLeaf.closedYaw == doorPlacement.closedYaw);
    assert(leftLeaf.openYaw == doorPlacement.closedYaw - doorPlacement.openAngle);
    assert(leftCollider.position == doorPlacement.leftHingePosition + glm::vec3(doorPlacement.leafScale.x * 0.5f, 0.0f, 0.0f));
    assert(leftCollider.halfExtents == doorPlacement.leafScale * 0.5f);
    assert(leftMesh.mesh == leftDoorMesh);

    const auto& rightLeaf = registry.get<DoorLeafComponent>(door.rightLeaf);
    const auto& rightCollider = registry.get<StaticColliderComponent>(door.rightLeaf);
    const auto& rightMesh = registry.get<MeshComponent>(door.rightLeaf);
    assert(rightLeaf.hingePosition == doorPlacement.rightHingePosition);
    assert(rightLeaf.openYaw == doorPlacement.closedYaw + doorPlacement.openAngle);
    assert(rightCollider.position == doorPlacement.rightHingePosition - glm::vec3(doorPlacement.leafScale.x * 0.5f, 0.0f, 0.0f));
    assert(rightMesh.mesh == rightDoorMesh);

    return 0;
}
