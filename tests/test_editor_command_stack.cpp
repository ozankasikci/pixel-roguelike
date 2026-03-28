#include "editor/EditorCommand.h"
#include "editor/EditorSceneDocument.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}

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
        assert(nearlyEqual(mesh.position.x, 0.0f));
        assert(!document.dirty());
    }

    {
        const bool redone = stack.redo(document);
        assert(redone);
        const auto* object = document.findObject(meshId);
        assert(object != nullptr);
        const auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        assert(nearlyEqual(mesh.position.x, 2.5f));
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

    return 0;
}
