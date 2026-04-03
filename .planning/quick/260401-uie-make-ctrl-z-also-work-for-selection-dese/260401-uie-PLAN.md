---
phase: quick
plan: 260401-uie
type: execute
wave: 1
depends_on: []
files_modified:
  - src/editor/core/EditorCommand.h
  - src/editor/core/EditorCommand.cpp
  - apps/level_editor/main.cpp
  - src/editor/ui/EditorOutlinerPanel.cpp
  - tests/editor/test_editor_command_stack.cpp
autonomous: true
requirements: []
must_haves:
  truths:
    - "Ctrl+Z after clicking an object restores the previous selection"
    - "Ctrl+Shift+Z after undoing a selection restores the new selection"
    - "Selection undo and transform undo interleave correctly on the same stack"
    - "Shift-click additive selection is undoable"
    - "Escape deselect-all is undoable"
    - "Outliner selection changes are undoable"
  artifacts:
    - path: "src/editor/core/EditorCommand.h"
      provides: "EditorSelectionCommand class and pushSelectionCommand on EditorCommandStack"
    - path: "src/editor/core/EditorCommand.cpp"
      provides: "EditorSelectionCommand implementation"
    - path: "apps/level_editor/main.cpp"
      provides: "Selection changes wrapped with before/after pushSelectionCommand calls"
  key_links:
    - from: "apps/level_editor/main.cpp"
      to: "EditorCommandStack::pushSelectionCommand"
      via: "capture before selection, mutate, push command"
      pattern: "pushSelectionCommand"
    - from: "EditorSelectionCommand::undo"
      to: "selectedIds vector"
      via: "pointer stored in command"
      pattern: "selectedIds_ ="
---

<objective>
Make Ctrl+Z / Ctrl+Shift+Z (undo/redo) work for selection and deselection changes in the level editor, matching Unity behavior. Currently only transform/document mutations are undoable. Selection changes (viewport click, shift-click, outliner click, Escape deselect) should push onto the same command stack so undo flows naturally between selection and transform commands.

Purpose: Editor UX improvement -- accidental deselection or wrong selection is currently unrecoverable without re-clicking.
Output: Selection commands on the undo stack, interleaved with existing document state commands.
</objective>

<context>
@src/editor/core/EditorCommand.h
@src/editor/core/EditorCommand.cpp
@src/editor/ui/EditorOutlinerPanel.h
@src/editor/ui/EditorOutlinerPanel.cpp
@apps/level_editor/main.cpp
@tests/editor/test_editor_command_stack.cpp

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/editor/core/EditorCommand.h:
```cpp
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    virtual const std::string& label() const = 0;
    virtual void undo(EditorSceneDocument& document) const = 0;
    virtual void redo(EditorSceneDocument& document) const = 0;
};

class EditorCommandStack {
public:
    bool canUndo() const;
    bool canRedo() const;
    bool undo(EditorSceneDocument& document);
    bool redo(EditorSceneDocument& document);
    bool pushDocumentStateCommand(std::string label,
                                  const EditorSceneDocumentState& beforeState,
                                  const EditorSceneDocumentState& afterState,
                                  EditorSceneDocument& document);
private:
    static constexpr std::size_t kMaxCommands = 256;
    void trimToLimit();
    std::vector<std::unique_ptr<IEditorCommand>> commands_;
    std::size_t cursor_ = 0;
};
```

From src/editor/ui/EditorOutlinerPanel.h (commandStack already a parameter):
```cpp
std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          EditorUiState& ui,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack);
```

Selection mutation helpers from EditorViewportInteraction.h:
```cpp
void toggleSelection(std::vector<std::uint64_t>& selectedIds, std::uint64_t id, bool additive);
void pruneSelection(const EditorSceneDocument& document, std::vector<std::uint64_t>& selectedIds);
void applySelectionHit(std::vector<std::uint64_t>& selectedIds,
                       const EditorSelectionPickerState& picker,
                       bool additive);
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create EditorSelectionCommand and pushSelectionCommand</name>
  <files>src/editor/core/EditorCommand.h, src/editor/core/EditorCommand.cpp</files>
  <action>
Create a new `EditorSelectionCommand` class implementing `IEditorCommand`.

In `EditorCommand.h`, add the following class after `EditorDocumentStateCommand` and before `EditorCommandStack`. Also add `#include <cstdint>` to the existing includes:

```cpp
class EditorSelectionCommand final : public IEditorCommand {
public:
    EditorSelectionCommand(std::string label,
                           std::vector<std::uint64_t>* selectedIds,
                           std::vector<std::uint64_t> beforeSelection,
                           std::vector<std::uint64_t> afterSelection);

    const std::string& label() const override { return label_; }
    void undo(EditorSceneDocument& document) const override;
    void redo(EditorSceneDocument& document) const override;

private:
    std::string label_;
    std::vector<std::uint64_t>* selectedIds_;
    std::vector<std::uint64_t> beforeSelection_;
    std::vector<std::uint64_t> afterSelection_;
};
```

In `EditorCommand.cpp`, implement the constructor and methods:
- Constructor: move all params into members, store pointer as-is.
- `undo()`: sets `*selectedIds_ = beforeSelection_` (ignores the document param, mark with `(void)document;`).
- `redo()`: sets `*selectedIds_ = afterSelection_` (same `(void)document;`).

Add a new public method to `EditorCommandStack` in the header:
```cpp
bool pushSelectionCommand(std::string label,
                          std::vector<std::uint64_t>* selectedIds,
                          const std::vector<std::uint64_t>& beforeSelection,
                          const std::vector<std::uint64_t>& afterSelection);
```

Implementation in .cpp:
- If `beforeSelection == afterSelection`, return false (no-op, don't pollute the stack).
- Otherwise, erase commands after cursor (same pattern as pushDocumentStateCommand at line 126-128):
  ```cpp
  if (cursor_ < commands_.size()) {
      commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
  }
  ```
- Push `std::make_unique<EditorSelectionCommand>(std::move(label), selectedIds, beforeSelection, afterSelection)`.
- Set `cursor_ = commands_.size()`.
- Call `trimToLimit()`.
- Do NOT call `syncDirtyFlags()` -- selection changes don't affect document dirty state. The dirty flag should only reflect unsaved scene/environment changes.
- Return true.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_editor_command_stack 2>&1 | tail -5 && ./build/tests/editor/test_editor_command_stack</automated>
  </verify>
  <done>EditorSelectionCommand class exists, pushSelectionCommand method exists on EditorCommandStack, existing tests still pass</done>
</task>

<task type="auto">
  <name>Task 2: Wrap selection mutation sites with undo commands</name>
  <files>apps/level_editor/main.cpp, src/editor/ui/EditorOutlinerPanel.cpp</files>
  <action>
There are several sites where `selectedIds` is mutated. Each needs a before-capture and a push-after pattern:

```cpp
const auto selectionBefore = selectedIds;
// ... existing selection mutation code ...
commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);
```

**Site 1: Viewport click selection** (main.cpp ~line 1685-1712)
Inside the `else if (renderViewportState.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !orbitModifierActive && !editorGizmoIsHot())` block:
- Add `const auto selectionBefore = selectedIds;` right at the top of this block (before the ray building at ~line 1689).
- After the entire `if (!hits.empty()) { ... } else { ... }` block (after line 1711), add:
  ```cpp
  commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);
  ```
  This covers click-to-select, shift-click-to-add, and click-on-empty-to-deselect.

**Site 2: Escape deselect** (main.cpp ~line 466-473)
Inside the Escape handler block:
- Add `const auto selectionBefore = selectedIds;` right before `selectedIds.clear()` (~line 470).
- After `selectionPicker.clear()` (~line 471), add:
  ```cpp
  commandStack.pushSelectionCommand("Deselect All", &selectedIds, selectionBefore, selectedIds);
  ```
  This will be a no-op if selection was already empty (pushSelectionCommand returns false for equal before/after).

**Site 3: Outliner click selection** (EditorOutlinerPanel.cpp ~lines 106-135)
The `renderOutliner` function already receives `EditorCommandStack& commandStack` as a parameter (no signature change needed).

Inside the per-object loop, the selection mutations happen in `if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())` (~line 106) and the right-click handler (~line 132). These are separate `if` blocks that both modify `selectedIds`.

Add `const auto selectionBefore = selectedIds;` right before the left-click check (before line 106). Then after both the left-click and right-click blocks (after line 135), add:
```cpp
commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);
```

**Sites to intentionally SKIP (no selection command needed):**

- **Place object auto-select** (main.cpp ~lines 1655, 1681): Already wrapped by document state commands for the placement. Adding a selection command would create confusing double-undo. `pruneSelection` on undo handles stale selection cleanup.

- **Duplicate auto-select** (main.cpp ~line 1850): Same -- duplicate already pushes a document state command.

- **Delete clear** (main.cpp ~line 1874): Paired with a document state command for deletion. `pruneSelection` on undo handles it.

- **Selection picker popup**: Comment at line 1715 confirms "Selection picker overlay removed -- hover highlight replaces it". `renderSelectionPicker` is no longer called. Skip.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -10</automated>
  </verify>
  <done>All user-initiated selection mutation sites (viewport click, shift-click, miss-click-to-deselect, Escape, outliner click) push selection commands onto the undo stack. Build succeeds with no errors.</done>
</task>

<task type="auto">
  <name>Task 3: Add selection undo test coverage</name>
  <files>tests/editor/test_editor_command_stack.cpp</files>
  <action>
Add test cases to `test_editor_command_stack.cpp` for the new selection undo functionality. Place these after the existing test blocks (before `return 0;`).

**Test case 1: Selection undo/redo roundtrip**
```cpp
{
    // Reset for selection tests
    EditorSceneDocument selDoc;
    selDoc.clear();
    const std::uint64_t selMeshId = selDoc.addMesh(makeMeshPlacement());
    EditorCommandStack selStack;
    selStack.reset(selDoc);
    std::vector<std::uint64_t> selectedIds;

    // Select the mesh
    const bool pushed = selStack.pushSelectionCommand("Select Mesh", &selectedIds, {}, {selMeshId});
    assert(pushed);
    assert(selectedIds.empty()); // pushSelectionCommand does not apply -- caller already mutated
    selectedIds = {selMeshId};   // simulate what the caller does before pushing

    // Actually: pushSelectionCommand stores before/after but the selectedIds vector was already
    // mutated by the caller. The command stores snapshots. Undo should restore.
    // Let's redo the test properly:
}
```

Actually, the correct test pattern reflects how the system actually works: the caller captures before, mutates selectedIds, then pushes the command with both states. The command stores the snapshots. On undo, it restores. Test this:

```cpp
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
```

**Test case 2: No-op selection not pushed**
```cpp
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
```

**Test case 3: Selection and document commands interleave**
```cpp
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

    // Now undo all three in reverse:
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
```

Follow the existing test style: plain `assert()` calls, no test framework, `#include "common/TestSupport.h"` already present.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_editor_command_stack 2>&1 | tail -5 && ./build/tests/editor/test_editor_command_stack</automated>
  </verify>
  <done>Selection undo tests pass, covering roundtrip, no-op rejection, and interleave with document state commands</done>
</task>

</tasks>

<verification>
1. Build: `cmake --build build --target level-editor` compiles without errors
2. Tests: `./build/tests/editor/test_editor_command_stack` passes (exit 0)
3. Manual: Launch editor, click object, Ctrl+Z -- selection reverts. Ctrl+Shift+Z -- re-selects. Move object with gizmo, Ctrl+Z -- object moves back, Ctrl+Z again -- selection reverts.
</verification>

<success_criteria>
- Selection changes (click, shift-click, Escape deselect, outliner click) are on the undo stack
- Undo/redo flows naturally between selection and transform commands
- Existing undo/redo for transforms still works unchanged
- No double-undo artifacts for placement/duplicate/delete (those already have document commands)
- Editor test suite passes
</success_criteria>

<output>
After completion, create `.planning/quick/260401-uie-make-ctrl-z-also-work-for-selection-dese/260401-uie-SUMMARY.md`
</output>
