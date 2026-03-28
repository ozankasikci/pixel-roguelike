#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"
#include "common/TestSupport.h"

#include <cassert>

namespace {

glm::vec3 computeMin(const RawMeshData& mesh) {
    assert(!mesh.positions.empty());
    glm::vec3 result = mesh.positions.front();
    for (const auto& position : mesh.positions) {
        result = glm::min(result, position);
    }
    return result;
}

glm::vec3 computeMax(const RawMeshData& mesh) {
    assert(!mesh.positions.empty());
    glm::vec3 result = mesh.positions.front();
    for (const auto& position : mesh.positions) {
        result = glm::max(result, position);
    }
    return result;
}

} // namespace

int main() {
    const RawMeshData gltf = ModelLoader::loadRaw(resolveProjectPath("assets/meshes/pillar.glb"));
    const RawMeshData fbx = ModelLoader::loadRaw(resolveProjectPath("tests/data/models/pillar_import_test.fbx"));

    assert(!gltf.positions.empty());
    assert(!gltf.indices.empty());
    assert(!fbx.positions.empty());
    assert(!fbx.indices.empty());

    assert(fbx.normals.size() == fbx.positions.size());
    assert(fbx.uvs.size() == fbx.positions.size());
    assert(fbx.tangents.size() == fbx.positions.size());

    const glm::vec3 gltfMin = computeMin(gltf);
    const glm::vec3 gltfMax = computeMax(gltf);
    const glm::vec3 fbxMin = computeMin(fbx);
    const glm::vec3 fbxMax = computeMax(fbx);

    assert(test_support::nearlyEqualVec3(gltfMin, fbxMin, 0.02f));
    assert(test_support::nearlyEqualVec3(gltfMax, fbxMax, 0.02f));
    return 0;
}
