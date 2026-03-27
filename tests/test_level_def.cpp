#include "game/level/LevelDef.h"

#include <algorithm>
#include <cassert>

int main() {
    const auto data = loadLevelDef(CATHEDRAL_SCENE_FILE);

    assert(data.environmentProfile == EnvironmentProfile::CathedralArcade);
    assert(data.meshes.size() == 126);
    assert(data.lights.size() == 13);
    assert(data.boxColliders.size() == 25);
    assert(data.cylinderColliders.size() == 14);
    assert(data.hasPlayerSpawn);
    assert(data.archetypes.size() == 2);

    assert(data.meshes.front().meshId == "plane");
    assert(data.meshes[2].position == glm::vec3(0.0f, 9.0f, -6.5f));
    assert(std::count_if(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.material.has_value() && *mesh.material == MaterialKind::Brick;
    }) == 15);
    assert(std::any_of(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.position == glm::vec3(-9.0f, 4.5f, -6.5f)
            && mesh.materialId == "brick_default";
    }));
    assert(std::count_if(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.material.has_value()
            && *mesh.material == MaterialKind::Wax
            && mesh.position.y > 3.5f;
    }) == 6);
    assert(std::any_of(data.meshes.begin(), data.meshes.end(), [](const LevelMeshPlacement& mesh) {
        return mesh.meshId == "cube"
            && mesh.position == glm::vec3(-5.8f, 0.07f, 2.8f)
            && mesh.material.has_value()
            && *mesh.material == MaterialKind::Wood
            && mesh.tint.has_value()
            && *mesh.tint == glm::vec3(0.60f, 0.45f, 0.29f);
    }));
    assert(data.meshes.back().meshId == "arch");
    assert(data.playerSpawn.position == glm::vec3(0.0f, 1.6f, 5.4f));
    assert(data.playerSpawn.fallRespawnY == -8.0f);
    assert(data.archetypes.front().archetypeId == "checkpoint_shrine");
    assert(data.archetypes.back().archetypeId == "cathedral_double_door");
    return 0;
}
