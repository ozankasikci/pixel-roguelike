#include "game/level/LevelDef.h"
#include "game/components/ColliderComponent.h"
#include "game/modules/checkpoint/CheckpointModule.h"

#include <algorithm>
#include <cassert>

int main() {
    registerCheckpointModule();
    const auto data = loadLevelDef(SILOS_CLOISTER_SCENE_FILE);

    assert(data.environmentProfile == EnvironmentProfile::Default);
    assert(data.hasPlayerSpawn);
    assert(data.playerSpawn.position == glm::vec3(0.0f, 1.6f, 10.1f));
    assert(data.lights.size() == 4);
    assert(std::count_if(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.meshId == "arch";
    }) == 13);
    assert(std::count_if(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.meshId == "pillar";
    }) == 16);
    assert(std::any_of(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.materialId == "cloister_stone";
    }));
    assert(std::any_of(data.colliders.begin(), data.colliders.end(), [](const LevelColliderPlacement& collider) {
        return collider.shape == ColliderShape::Box
            && collider.position == glm::vec3(0.0f, -0.1f, 0.0f)
            && collider.halfExtents == glm::vec3(10.45f, 0.1f, 10.45f);
    }));
    assert(std::count_if(data.colliders.begin(), data.colliders.end(), [](const LevelColliderPlacement& c) {
        return c.shape == ColliderShape::Cylinder;
    }) >= 18);
    assert(data.checkpoints.size() == 1);
    assert(data.checkpoints.front().name == "shrine");

    return 0;
}
