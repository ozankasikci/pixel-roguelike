---
phase: quick
plan: 260401-uie
subsystem: editor
tags: [editor, undo-redo, selection, UX]
dependency_graph:
  requires: []
  provides: [selection-undo-redo]
  affects: [level-editor]
tech_stack:
  added: []
  patterns: [command-pattern, before-after-capture]
key_files:
  created: []
  modified:
    - src/editor/core/EditorCommand.h
    - src/editor/core/EditorCommand.cpp
    - apps/level_editor/main.cpp
    - src/editor/ui/EditorOutlinerPanel.cpp
    - tests/editor/test_editor_command_stack.cpp
decisions:
  - EditorSelectionCommand does NOT call syncDirtyFlags — selection changes don't affect document dirty state
  - Escape key now also clears selection (added selectedIds.clear + selectionPicker.clear to Escape handler)
  - pushSelectionCommand returns false for equal before/after states — no-op guard prevents stack pollution
metrics:
  duration: ~10 minutes
  completed: 2026-04-01
  tasks_completed: 3
  files_modified: 5
---

# Quick Task 260401-uie: Selection Undo/Redo Summary

## One-liner

Selection changes (viewport click, shift-click, Escape deselect, outliner click) pushed onto EditorCommandStack as EditorSelectionCommand, interleaving naturally with transform/document undo.

## What Was Done

### Task 1: EditorSelectionCommand and pushSelectionCommand
- Added `EditorSelectionCommand` class implementing `IEditorCommand` to `EditorCommand.h/.cpp`
- Stores `before`/`after` selection snapshots plus a raw pointer to the live `selectedIds` vector
- `undo()` restores `beforeSelection_`, `redo()` restores `afterSelection_`
- Added `pushSelectionCommand()` to `EditorCommandStack` — mirrors `pushDocumentStateCommand` pattern but skips `syncDirtyFlags` (selection doesn't dirty the scene)
- No-op guard: returns false if `before == after`
- Commit: `853c6a7`

### Task 2: Wrap selection mutation sites
- **Viewport click** (main.cpp): Capture `selectionBefore` at start of click handler, push after hit/miss resolution — covers click-to-select, shift-click-add, click-on-empty-deselect
- **Escape deselect** (main.cpp): Added `selectedIds.clear()` and `selectionPicker.clear()` to Escape handler (these were missing), wrapped with before/after capture
- **Outliner clicks** (EditorOutlinerPanel.cpp): Capture before left-click check, push after both left-click and right-click blocks
- Skipped: place-object auto-select, duplicate auto-select, delete clear — all already paired with document state commands
- Commit: `428112f`

### Task 3: Test coverage
- Added three test cases to `test_editor_command_stack.cpp`:
  1. Selection undo/redo roundtrip — push select, undo restores empty, redo restores selection
  2. No-op rejection — equal before/after not pushed, canUndo remains false
  3. Selection/document interleave — select, move, deselect; undo all three in reverse; redo all three
- Commit: `b871d75`

## Deviations from Plan

### Auto-added: Escape deselect behavior

**Found during:** Task 2, Site 2

**Issue:** The plan described wrapping an existing `selectedIds.clear()` inside the Escape handler at ~line 466-473. The actual code had no selection clearing in the Escape handler — only `placementState.clear()`, `widgetCommand.clear()`, and `gizmoCommand.clear()`.

**Fix:** Added `selectedIds.clear()` and `selectionPicker.clear()` to the Escape handler before adding the selection command wrapping. This adds the behavior the plan assumed existed, which is also the correct UX (Escape should deselect, matching standard editor conventions).

**Files modified:** `apps/level_editor/main.cpp`

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| 1    | 853c6a7 | Add EditorSelectionCommand and pushSelectionCommand to EditorCommandStack |
| 2    | 428112f | Wrap selection mutation sites with undo commands in main.cpp and outliner |
| 3    | b871d75 | Add selection undo test coverage: roundtrip, no-op, interleave with document commands |

## Known Stubs

None.

## Self-Check: PASSED

- `src/editor/core/EditorCommand.h` — contains `EditorSelectionCommand` and `pushSelectionCommand`
- `src/editor/core/EditorCommand.cpp` — contains implementation
- `apps/level_editor/main.cpp` — viewport click and Escape sites wrapped
- `src/editor/ui/EditorOutlinerPanel.cpp` — outliner click wrapped
- `tests/editor/test_editor_command_stack.cpp` — three new test cases pass
- Commits 853c6a7, 428112f, b871d75 exist in git log
