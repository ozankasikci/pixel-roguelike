#include "game/prefabs/GameplayPrefabAssets.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
    return nearlyEqual(a.x, b.x, epsilon)
        && nearlyEqual(a.y, b.y, epsilon)
        && nearlyEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    const auto checkpointAsset = loadGameplayPrefabAsset(CHECKPOINT_PREFAB_FILE);
    assert(checkpointAsset.type == GameplayPrefabType::Checkpoint);
    assert(checkpointAsset.checkpoint.position == glm::vec3(0.0f, 1.3f, 0.0f));
    assert(checkpointAsset.checkpoint.respawnPosition == glm::vec3(0.0f, 1.6f, 2.5f));

    const auto checkpointInstance = instantiateGameplayPrefab(
        checkpointAsset,
        glm::vec3(0.0f, 0.0f, -35.3f),
        0.0f
    );
    assert(checkpointInstance.type == GameplayPrefabType::Checkpoint);
    assert(checkpointInstance.checkpoint.position == glm::vec3(0.0f, 1.3f, -35.3f));
    assert(checkpointInstance.checkpoint.respawnPosition == glm::vec3(0.0f, 1.6f, -32.8f));
    assert(checkpointInstance.checkpoint.lightPosition == glm::vec3(0.0f, 1.95f, -36.3f));

    const auto doorAsset = loadGameplayPrefabAsset(DOUBLE_DOOR_PREFAB_FILE);
    assert(doorAsset.type == GameplayPrefabType::DoubleDoor);
    assert(doorAsset.doubleDoor.rootPosition == glm::vec3(0.0f, 3.13f, 0.0f));
    assert(doorAsset.doubleDoor.leftHingePosition == glm::vec3(-2.35f, 3.13f, -0.88f));

    const auto rotatedDoor = instantiateGameplayPrefab(
        doorAsset,
        glm::vec3(10.0f, 0.0f, 5.0f),
        90.0f
    );
    assert(rotatedDoor.type == GameplayPrefabType::DoubleDoor);
    assert(nearlyEqualVec3(rotatedDoor.doubleDoor.rootPosition, glm::vec3(10.0f, 3.13f, 5.0f)));
    assert(nearlyEqualVec3(rotatedDoor.doubleDoor.leftHingePosition, glm::vec3(10.88f, 3.13f, 2.65f)));
    assert(nearlyEqualVec3(rotatedDoor.doubleDoor.rightHingePosition, glm::vec3(10.88f, 3.13f, 7.35f)));
    assert(nearlyEqual(rotatedDoor.doubleDoor.closedYaw, 90.0f));

    return 0;
}
