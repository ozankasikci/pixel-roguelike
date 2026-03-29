#include "game/level/LevelDef.h"

#include <algorithm>
#include <cassert>

int main() {
    const auto data = loadLevelDef(SILOS_CLOISTER_SCENE_FILE);

    assert(data.environmentProfile == EnvironmentProfile::CloisterDaylight);
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
    assert(std::any_of(data.boxColliders.begin(), data.boxColliders.end(), [](const LevelBoxColliderPlacement& collider) {
        return collider.position == glm::vec3(0.0f, -0.1f, 0.0f)
            && collider.halfExtents == glm::vec3(10.45f, 0.1f, 10.45f);
    }));
    assert(data.cylinderColliders.size() >= 18);
    assert(std::any_of(data.archetypes.begin(), data.archetypes.end(), [](const LevelArchetypePlacement& archetype) {
        return archetype.archetypeId == "checkpoint_shrine";
    }));

    return 0;
}
