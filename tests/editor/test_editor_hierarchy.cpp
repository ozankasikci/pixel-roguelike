#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/EditorOutlinerPanel.h"
#include "editor/ui/LevelEditorUi.h"
#include "common/TestSupport.h"

#include <cassert>

namespace {

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
    return placement;
}

} // namespace

int main() {
    EditorSceneDocument document;
    document.clear();

    const std::uint64_t parentId = document.addMesh(makeMesh(glm::vec3(0.0f)));
    const std::uint64_t childId = document.addMesh(makeMesh(glm::vec3(1.0f, 0.0f, 0.0f)));

    const glm::vec3 childWorldBeforeParenting = translationOf(document.worldTransformMatrix(childId));
    assert(test_support::nearlyEqualVec3(childWorldBeforeParenting, glm::vec3(1.0f, 0.0f, 0.0f)));

    assert(document.canSetParent(childId, parentId));
    assert(document.setParent(childId, parentId));
    assert(document.parentObjectId(childId) == parentId);
    assert(test_support::nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), childWorldBeforeParenting));
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

    assert(test_support::nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), glm::vec3(5.0f, 0.0f, -2.0f)));

    assert(document.clearParent(childId));
    assert(document.parentObjectId(childId) == 0);
    assert(test_support::nearlyEqualVec3(translationOf(document.worldTransformMatrix(childId)), glm::vec3(5.0f, 0.0f, -2.0f)));

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
    assert(test_support::nearlyEqualVec3(translationOf(reorderDocument.worldTransformMatrix(childOfA)), childWorld));

    EditorSceneDocument navDocument;
    navDocument.clear();
    const std::uint64_t navRootA = navDocument.addMesh(makeMesh(glm::vec3(0.0f)));
    const std::uint64_t navRootB = navDocument.addMesh(makeMesh(glm::vec3(3.0f, 0.0f, 0.0f)));
    const std::uint64_t navChild = navDocument.addMesh(makeMesh(glm::vec3(1.0f, 0.0f, 0.0f)));
    assert(navDocument.setParent(navChild, navRootA));

    EditorUiState ui;
    std::vector<std::uint64_t> selectedIds{navRootA};

    auto visibleRows = buildOutlinerVisibleRows(navDocument, ui);
    assert(visibleRows.size() == 2);
    assert(visibleRows[0].objectId == navRootA);
    assert(visibleRows[1].objectId == navRootB);

    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Right));
    assert(ui.expandedOutlinerIds.contains(navRootA));
    visibleRows = buildOutlinerVisibleRows(navDocument, ui);
    assert(visibleRows.size() == 3);
    assert(visibleRows[1].objectId == navChild);

    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Down));
    assert(selectedIds.size() == 1);
    assert(selectedIds[0] == navChild);

    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Left));
    assert(selectedIds.size() == 1);
    assert(selectedIds[0] == navRootA);

    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Left));
    assert(!ui.expandedOutlinerIds.contains(navRootA));
    visibleRows = buildOutlinerVisibleRows(navDocument, ui);
    assert(visibleRows.size() == 2);

    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Down));
    assert(selectedIds.size() == 1);
    assert(selectedIds[0] == navRootB);

    ui.expandedOutlinerIds.insert(navRootA);
    selectedIds = {navRootA};
    ui.outlinerAnchorId = navRootA;
    assert(applyOutlinerKeyboardNavigation(navDocument, ui, selectedIds, OutlinerNavDirection::Down, true));
    assert(selectedIds.size() == 2);
    assert(selectedIds[0] == navRootA);
    assert(selectedIds[1] == navChild);

    return 0;
}
