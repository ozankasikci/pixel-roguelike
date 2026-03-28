#include "editor/EditorSceneDocument.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}

bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b) {
    return nearlyEqual(a.x, b.x) && nearlyEqual(a.y, b.y) && nearlyEqual(a.z, b.z);
}

glm::vec3 translationOf(const glm::mat4& matrix) {
    return glm::vec3(matrix[3]);
}

LevelMeshPlacement makeMesh(const glm::vec3& position) {
    LevelMeshPlacement placement;
    placement.meshId = "cube";
    placement.position = position;
    placement.scale = glm::vec3(1.0f);
    placement.rotation = glm::vec3(0.0f);
    placement.materialId = "stone_default";
    placement.material = MaterialKind::Stone;
    return placement;
}

} // namespace

int main() {
    EditorSceneDocument document;
    document.clear();

    const std::uint64_t parentId = document.addMesh(makeMesh(glm::vec3(0.0f)));
    const std::uint64_t childId = document.addMesh(makeMesh(glm::vec3(1.0f, 0.0f, 0.0f)));

    const glm::vec3 childWorldBeforeParenting = translationOf(document.worldTransformMatrix(childId));
    assert(nearlyEqualVec3(childWorldBeforeParenting, glm::vec3(1.0f, 0.0f, 0.0f)));

    assert(document.canSetParent(childId, parentId));
    assert(document.setParent(childId, parentId));
    assert(document.parentObjectId(childId) == parentId);
    assert(nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), childWorldBeforeParenting));
    assert(!document.canSetParent(parentId, childId));

    {
        auto* parentObject = document.findObject(parentId);
        assert(parentObject != nullptr);
        auto& mesh = std::get<LevelMeshPlacement>(parentObject->payload);
        mesh.rotation.y = 90.0f;
        mesh.scale = glm::vec3(2.0f);
        mesh.position = glm::vec3(5.0f, 0.0f, 0.0f);
        document.markSceneDirty();
    }

    assert(nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), glm::vec3(5.0f, 0.0f, -2.0f)));

    assert(document.clearParent(childId));
    assert(document.parentObjectId(childId) == 0);
    assert(nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), glm::vec3(5.0f, 0.0f, -2.0f)));

    {
        const auto* childObject = document.findObject(childId);
        assert(childObject != nullptr);
        const auto& mesh = std::get<LevelMeshPlacement>(childObject->payload);
        assert(mesh.parentNodeId.empty());
    }

    EditorSceneDocument reorderDocument;
    reorderDocument.clear();
    const std::uint64_t rootA = reorderDocument.addMesh(makeMesh(glm::vec3(0.0f)));
    const std::uint64_t rootB = reorderDocument.addMesh(makeMesh(glm::vec3(3.0f, 0.0f, 0.0f)));
    const std::uint64_t childOfA = reorderDocument.addMesh(makeMesh(glm::vec3(1.0f, 0.0f, 0.0f)));
    assert(reorderDocument.setParent(childOfA, rootA));
    const glm::vec3 childWorld = translationOf(reorderDocument.worldTransformMatrix(childOfA));

    assert(reorderDocument.moveObjectBefore(childOfA, rootB));
    assert(reorderDocument.parentObjectId(childOfA) == 0);
    const auto roots = reorderDocument.rootObjectIds();
    assert(roots.size() == 3);
    assert(roots[0] == rootA);
    assert(roots[1] == childOfA);
    assert(roots[2] == rootB);
    assert(nearlyEqualVec3(translationOf(reorderDocument.worldTransformMatrix(childOfA)), childWorld));

    return 0;
}
