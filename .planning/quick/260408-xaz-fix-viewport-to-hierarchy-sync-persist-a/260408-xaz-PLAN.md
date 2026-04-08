---
phase: quick
plan: 260408-xaz
type: execute
wave: 1
depends_on: []
files_modified:
  - src/editor/ui/EditorOutlinerPanel.cpp
autonomous: true
must_haves:
  truths:
    - "Clicking an entity in the viewport expands its ancestors in the outliner and they stay expanded on subsequent frames"
    - "Manually collapsed ancestors remain collapsed (toggle still works)"
  artifacts:
    - path: "src/editor/ui/EditorOutlinerPanel.cpp"
      provides: "Persistent ancestor expansion on scroll-to-selection"
      contains: "ui.expandedOutlinerIds.insert(ancestorId)"
  key_links:
    - from: "scrollToSelection flag"
      to: "ui.expandedOutlinerIds"
      via: "ancestor-walk loop persists expansion"
      pattern: "expandedOutlinerIds\\.insert\\(ancestorId\\)"
---

<objective>
Fix viewport-to-hierarchy sync so that ancestor tree nodes persist their expanded state after a scroll-to-selection event.

Purpose: When clicking an entity in the viewport, the outliner scrolls to it and expands ancestors — but those ancestors collapse on the next frame because the expansion is only stored in a temporary per-frame set (`expandForScroll`), not in the persistent `ui.expandedOutlinerIds`.

Output: One-line fix in EditorOutlinerPanel.cpp that inserts ancestors into the persistent expanded set.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/editor/ui/EditorOutlinerPanel.cpp
@src/editor/ui/LevelEditorUi.h
</context>

<tasks>

<task type="auto">
  <name>Task 1: Persist ancestor expansion in expandedOutlinerIds during scroll-to-selection</name>
  <files>src/editor/ui/EditorOutlinerPanel.cpp</files>
  <action>
In EditorOutlinerPanel.cpp, inside the `if (ui.scrollToSelection)` block (around line 312-320), add a single line to persist each ancestor into `ui.expandedOutlinerIds` alongside the temporary `expandForScroll` set.

Change the ancestor-walk loop from:

```cpp
while (ancestorId != 0) {
    expandForScroll.insert(ancestorId);
    ancestorId = document.parentObjectId(ancestorId);
}
```

To:

```cpp
while (ancestorId != 0) {
    expandForScroll.insert(ancestorId);
    ui.expandedOutlinerIds.insert(ancestorId);
    ancestorId = document.parentObjectId(ancestorId);
}
```

The `expandForScroll` set is still needed — it drives `shouldOpen` on line 343-345 for the current frame before `expandedOutlinerIds` would naturally take effect. The new line ensures that on the next frame (when `scrollToSelection` is false and `expandForScroll` is empty), the ancestors remain open via the persistent set.

Do NOT modify any other logic. The existing toggle handler (lines 352-358) already handles manual collapse — if the user clicks the arrow to collapse an ancestor, `ui.expandedOutlinerIds.erase(objectId)` fires correctly.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>EditorOutlinerPanel.cpp compiles cleanly. The ancestor-walk loop now inserts into both expandForScroll (current-frame rendering) and ui.expandedOutlinerIds (persistent state). Viewport click -> outliner scroll will keep ancestors expanded across frames.</done>
</task>

</tasks>

<verification>
Build the level-editor target and confirm no compilation errors. Optionally launch the editor, click an entity nested inside a group in the viewport, and confirm the outliner expands to show it and stays expanded.
</verification>

<success_criteria>
- level-editor compiles without errors or warnings related to EditorOutlinerPanel
- Ancestor nodes expanded by scroll-to-selection persist across frames (do not collapse on next render)
- Manual toggle (clicking the tree arrow) still collapses/expands as before
</success_criteria>

<output>
After completion, create `.planning/quick/260408-xaz-fix-viewport-to-hierarchy-sync-persist-a/260408-xaz-SUMMARY.md`
</output>
