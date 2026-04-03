---
phase: quick
plan: 260403-wsw
type: execute
wave: 1
depends_on: []
files_modified:
  - src/editor/ui/EditorOutlinerPanel.cpp
  - src/editor/ui/LevelEditorUi.h
autonomous: true
requirements: []
---

<objective>
Enable arrow-key navigation in the hierarchy panel (`Outliner`) so selection can move without the mouse.

Purpose: Make hierarchy traversal faster and keyboard-friendly.
Output: Outliner keyboard handling wired into existing selection and scroll behavior.
</objective>

<context>
@src/editor/ui/EditorOutlinerPanel.cpp
@src/editor/ui/LevelEditorUi.h
@src/editor/ui/EditorAssetBrowserPanel.cpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add focused arrow-key navigation to the outliner</name>
  <files>src/editor/ui/EditorOutlinerPanel.cpp, src/editor/ui/LevelEditorUi.h</files>
  <action>
Keep the change local to the outliner. Reuse the existing visible-order traversal in `EditorOutlinerPanel.cpp` as the source of truth for keyboard movement.

When the Outliner window is focused, no text field is active, and no popup is open:
- `UpArrow` / `DownArrow`: move the active selection to the previous or next visible row, clamp at the ends, update `selectedIds`, `ui.outlinerAnchorId`, `ui.inspectorContext`, and set `ui.scrollToSelection = true`.
- `Shift+UpArrow` / `Shift+DownArrow`: extend the selection range from `ui.outlinerAnchorId` using the same visible-order list instead of inventing a second selection model.
- `RightArrow`: if the selected node has children and is collapsed, expand it; if already expanded, move selection to the first visible child.
- `LeftArrow`: if the selected node is expanded, collapse it; otherwise move selection to its parent.

If left/right needs a small pending open/close request, add the minimum state to `EditorUiState` in `LevelEditorUi.h` rather than introducing a new subsystem.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor</automated>
  </verify>
  <done>
    The hierarchy panel supports keyboard row navigation, selection stays synced with the inspector, and the selected row scrolls into view as it changes.
  </done>
</task>

</tasks>

<verification>
- Build `level-editor`.
- Manual check: click the Outliner, use arrow keys to move selection, confirm `Shift+Arrow` range select works, and verify `Left/Right` collapse or expand hierarchy nodes without breaking mouse selection.
</verification>

<success_criteria>
The Outliner can be navigated from the keyboard alone, and its selection, scroll, and expand/collapse behavior remain consistent with the existing mouse-driven workflow.
</success_criteria>

<output>
After completion, create `.planning/quick/260403-wsw-enable-arrow-key-navigation-in-the-hiera/260403-wsw-SUMMARY.md`
</output>
