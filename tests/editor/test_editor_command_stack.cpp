#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "common/TestSupport.h"

#include <cassert>

namespace {

LevelMeshPlacement makeMeshPlacement() {
    LevelMeshPlacement placement;
    placement.meshId = "cube";
    placement.materialId = "stone_default";
    placement.position = glm::vec3(0.0f);
    placement.scale = glm::vec3(1.0f);
    placement.rotation = glm::vec3(0.0f);
    return placement;
}

} // namespace

int main() {
    EditorSceneDocument document;
    document.clear();

    const std::uint64_t meshId = document.addMesh(makeMeshPlacement());
    EditorCommandStack stack;
    stack.reset(document);

    {
        const EditorSceneDocumentState before = document.captureState();
        auto* object = document.findObject(meshId);
        assert(object != nullptr);
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        mesh.position.x = 2.5f;
        document.markSceneDirty();
        const bool pushed = stack.pushDocumentStateCommand("Move Mesh", before, document.captureState(), document);
        assert(pushed);
    }

    assert(document.dirty());
    assert(stack.canUndo());
    assert(!stack.canRedo());
    assert(std::string(stack.undoLabel()) == "Move Mesh");

    {
        const bool undone = stack.undo(document);
        assert(undone);
        const auto* object = document.findObject(meshId);
        assert(object != nullptr);
        const auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        assert(test_support::nearlyEqual(mesh.position.x, 0.0f));
        assert(!document.dirty());
    }

    {
        const bool redone = stack.redo(document);
        assert(redone);
        const auto* object = document.findObject(meshId);
        assert(object != nullptr);
        const auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        assert(test_support::nearlyEqual(mesh.position.x, 2.5f));
        assert(document.dirty());
    }

    stack.markSaved(document);
    assert(!document.dirty());

    {
        const bool undone = stack.undo(document);
        assert(undone);
        assert(document.dirty());
        const bool redone = stack.redo(document);
        assert(redone);
        assert(!document.dirty());
    }

    {
        const bool undone = stack.undo(document);
        assert(undone);
        const EditorSceneDocumentState before = document.captureState();
        auto* object = document.findObject(meshId);
        assert(object != nullptr);
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        mesh.position.z = -3.0f;
        document.markSceneDirty();
        const bool pushed = stack.pushDocumentStateCommand("Move Mesh Again", before, document.captureState(), document);
        assert(pushed);
        assert(stack.canUndo());
        assert(!stack.canRedo());
        assert(std::string(stack.undoLabel()) == "Move Mesh Again");
    }

    // Selection undo/redo roundtrip
    {
        EditorSceneDocument selDoc;
        selDoc.clear();
        const std::uint64_t objId = selDoc.addMesh(makeMeshPlacement());
        EditorCommandStack selStack;
        selStack.reset(selDoc);
        std::vector<std::uint64_t> selectedIds;

        // Simulate: select the object
        const auto before1 = selectedIds;
        selectedIds = {objId};
        const bool pushed1 = selStack.pushSelectionCommand("Select", &selectedIds, before1, selectedIds);
        assert(pushed1);
        assert(selStack.canUndo());
        assert(selectedIds.size() == 1 && selectedIds[0] == objId);

        // Undo: should restore empty selection
        const bool undone1 = selStack.undo(selDoc);
        assert(undone1);
        assert(selectedIds.empty());
        assert(selStack.canRedo());

        // Redo: should restore selection
        const bool redone1 = selStack.redo(selDoc);
        assert(redone1);
        assert(selectedIds.size() == 1 && selectedIds[0] == objId);
    }

    // No-op selection not pushed
    {
        EditorSceneDocument selDoc;
        selDoc.clear();
        selDoc.addMesh(makeMeshPlacement());
        EditorCommandStack selStack;
        selStack.reset(selDoc);
        std::vector<std::uint64_t> selectedIds;

        const bool pushed = selStack.pushSelectionCommand("Noop", &selectedIds, {}, {});
        assert(!pushed);
        assert(!selStack.canUndo());
    }

    // Selection and document commands interleave
    {
        EditorSceneDocument selDoc;
        selDoc.clear();
        const std::uint64_t objId = selDoc.addMesh(makeMeshPlacement());
        EditorCommandStack selStack;
        selStack.reset(selDoc);
        std::vector<std::uint64_t> selectedIds;

        // Step 1: Select object
        selectedIds = {objId};
        selStack.pushSelectionCommand("Select", &selectedIds, {}, {objId});

        // Step 2: Move object (document state change)
        const EditorSceneDocumentState beforeMove = selDoc.captureState();
        auto* obj = selDoc.findObject(objId);
        assert(obj != nullptr);
        std::get<LevelMeshPlacement>(obj->payload).position.x = 5.0f;
        selDoc.markSceneDirty();
        selStack.pushDocumentStateCommand("Move", beforeMove, selDoc.captureState(), selDoc);

        // Step 3: Deselect
        selectedIds.clear();
        selStack.pushSelectionCommand("Deselect", &selectedIds, {objId}, {});

        // Undo all three in reverse:
        // Undo deselect -> selection restored
        selStack.undo(selDoc);
        assert(selectedIds.size() == 1 && selectedIds[0] == objId);

        // Undo move -> position back to 0
        selStack.undo(selDoc);
        obj = selDoc.findObject(objId);
        assert(obj != nullptr);
        assert(test_support::nearlyEqual(std::get<LevelMeshPlacement>(obj->payload).position.x, 0.0f));

        // Undo select -> empty selection
        selStack.undo(selDoc);
        assert(selectedIds.empty());

        // Redo all three
        selStack.redo(selDoc); // select
        assert(selectedIds.size() == 1 && selectedIds[0] == objId);
        selStack.redo(selDoc); // move
        obj = selDoc.findObject(objId);
        assert(test_support::nearlyEqual(std::get<LevelMeshPlacement>(obj->payload).position.x, 5.0f));
        selStack.redo(selDoc); // deselect
        assert(selectedIds.empty());
    }

    return 0;
}
