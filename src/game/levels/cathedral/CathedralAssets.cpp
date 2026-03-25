#include "game/levels/cathedral/CathedralAssets.h"

#include "engine/rendering/MeshGeometry.h"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

glm::mat4 makeModel(const glm::vec3& position,
                    const glm::vec3& scale,
                    const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

std::unique_ptr<Mesh> createWoodDoorLeafMesh(bool leftLeaf) {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;
    const float hand = leftLeaf ? 1.0f : -1.0f;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    addBox(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.74f));

    addBox(glm::vec3(0.0f, 0.44f, 0.08f), glm::vec3(0.94f, 0.08f, 0.18f));
    addBox(glm::vec3(0.0f, -0.44f, 0.08f), glm::vec3(0.94f, 0.08f, 0.18f));
    addBox(glm::vec3(-0.43f, 0.0f, 0.08f), glm::vec3(0.08f, 0.84f, 0.18f));
    addBox(glm::vec3(0.43f, 0.0f, 0.08f), glm::vec3(0.08f, 0.84f, 0.18f));

    for (float x : {-0.25f, -0.02f, 0.22f}) {
        addBox(glm::vec3(x, 0.0f, 0.12f), glm::vec3(0.18f, 0.82f, 0.10f));
    }

    for (float x : {-0.28f, -0.04f, 0.20f}) {
        addBox(glm::vec3(x, 0.0f, -0.12f), glm::vec3(0.17f, 0.78f, 0.08f));
    }

    addBox(glm::vec3(0.0f, 0.24f, 0.16f), glm::vec3(0.78f, 0.08f, 0.08f));
    addBox(glm::vec3(0.0f, -0.18f, 0.16f), glm::vec3(0.78f, 0.08f, 0.08f));
    addBox(glm::vec3(0.02f * hand, 0.05f, 0.18f), glm::vec3(0.92f, 0.08f, 0.08f), glm::vec3(0.0f, 0.0f, 31.0f * hand));
    addBox(glm::vec3(-0.02f * hand, -0.03f, -0.18f), glm::vec3(0.92f, 0.08f, 0.08f), glm::vec3(0.0f, 0.0f, -31.0f * hand));

    const float meetingEdgeX = 0.46f * hand;
    addBox(glm::vec3(meetingEdgeX, 0.0f, 0.16f), glm::vec3(0.05f, 0.90f, 0.12f));
    addBox(glm::vec3(meetingEdgeX - 0.03f * hand, 0.0f, 0.22f), glm::vec3(0.018f, 0.90f, 0.08f));
    addBox(glm::vec3(meetingEdgeX - 0.055f * hand, 0.0f, 0.10f), glm::vec3(0.012f, 0.86f, 0.06f));

    addBox(glm::vec3(-0.39f * hand, 0.0f, 0.18f), glm::vec3(0.06f, 0.84f, 0.10f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.indices);
}

} // namespace

void registerCathedralAssets(MeshLibrary& meshLibrary) {
    meshLibrary.registerDefaults();
    meshLibrary.registerMesh("door_leaf_left", createWoodDoorLeafMesh(true));
    meshLibrary.registerMesh("door_leaf_right", createWoodDoorLeafMesh(false));
    meshLibrary.loadFromFile("pillar", "assets/meshes/pillar.glb");
    meshLibrary.loadFromFile("arch", "assets/meshes/arch.glb");
    meshLibrary.loadFromFile("hand", "assets/meshes/hand_with_old_dagger.glb");
}
