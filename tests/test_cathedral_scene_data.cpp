#include "game/levels/cathedral/CathedralSceneData.h"

#include <cassert>
#include <string>

int main() {
    const auto data = loadCathedralSceneData(CATHEDRAL_SCENE_FILE);

    assert(data.meshes.size() == 44);
    assert(data.lights.size() == 10);
    assert(data.boxColliders.size() == 18);
    assert(data.cylinderColliders.size() == 14);
    assert(data.hasPlayerSpawn);
    assert(data.checkpoints.size() == 1);
    assert(data.doors.size() == 1);

    assert(data.meshes.front().meshName == "cube");
    assert(data.meshes.front().position == glm::vec3(-5.8f, 0.07f, 2.8f));
    assert(data.meshes[6].meshName == "cylinder");
    assert(data.meshes[24].position == glm::vec3(0.0f, 0.14f, -17.3f));
    assert(data.meshes.back().meshName == "arch");
    assert(data.meshes.back().position == glm::vec3(0.0f, 0.0f, -19.95f));

    assert(data.lights.front().position == glm::vec3(0.0f, 7.7f, 5.7f));
    assert(data.lights.front().color == glm::vec3(0.62f, 0.68f, 0.82f));
    assert(data.lights.back().position == glm::vec3(3.4f, 3.2f, -33.4f));

    assert(data.boxColliders.front().position == glm::vec3(0.0f, -0.05f, -6.5f));
    assert(data.boxColliders.front().halfExtents == glm::vec3(9.0f, 0.05f, 14.0f));
    assert(data.cylinderColliders.front().position == glm::vec3(-3.45f, 4.5f, 4.4f));
    assert(data.cylinderColliders.front().radius == 0.42f);
    assert(data.playerSpawn.position == glm::vec3(0.0f, 1.6f, 5.4f));
    assert(data.playerSpawn.fallRespawnY == -8.0f);
    assert(data.checkpoints.front().position == glm::vec3(0.0f, 1.3f, -35.3f));
    assert(data.checkpoints.front().respawnPosition == glm::vec3(0.0f, 1.6f, -32.8f));
    assert(data.doors.front().rootPosition == glm::vec3(0.0f, 3.13f, -19.4f));
    assert(data.doors.front().leafScale == glm::vec3(2.315f, 6.26f, 0.30f));

    return 0;
}
