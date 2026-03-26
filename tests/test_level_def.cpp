#include "game/level/LevelDef.h"

#include <cassert>

int main() {
    const auto data = loadLevelDef(CATHEDRAL_SCENE_FILE);

    assert(data.meshes.size() == 156);
    assert(data.lights.size() == 10);
    assert(data.boxColliders.size() == 25);
    assert(data.cylinderColliders.size() == 14);
    assert(data.hasPlayerSpawn);
    assert(data.archetypes.size() == 2);

    assert(data.meshes.front().meshId == "plane");
    assert(data.meshes[2].position == glm::vec3(0.0f, 9.0f, -6.5f));
    assert(data.meshes[111].position == glm::vec3(0.0f, 0.67f, -20.1f));
    assert(data.meshes[112].meshId == "cube");
    assert(data.meshes[112].material.has_value());
    assert(*data.meshes[112].material == MaterialKind::Wood);
    assert(data.meshes[112].tint.has_value());
    assert(*data.meshes[112].tint == glm::vec3(0.60f, 0.45f, 0.29f));
    assert(data.meshes.back().meshId == "arch");
    assert(data.playerSpawn.position == glm::vec3(0.0f, 1.6f, 5.4f));
    assert(data.playerSpawn.fallRespawnY == -8.0f);
    assert(data.archetypes.front().archetypeId == "checkpoint_shrine");
    assert(data.archetypes.back().archetypeId == "cathedral_double_door");

    return 0;
}
