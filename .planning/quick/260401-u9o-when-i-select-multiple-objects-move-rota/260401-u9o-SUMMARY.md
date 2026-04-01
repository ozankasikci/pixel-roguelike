---
phase: quick
plan: 260401-u9o
subsystem: editor/viewport
tags: [editor, gizmo, multi-select, transform]
dependency_graph:
  requires: []
  provides: [multi-selection gizmo manipulation]
  affects: [level-editor]
tech_stack:
  added: []
  patterns: [pivot-transform formula for centroid-relative manipulation]
key_files:
  modified:
    - src/editor/viewport/EditorViewportInteraction.cpp
decisions:
  - "Used unified pivot-transform formula (T(centroid) * localDelta * T(-centroid) * objectWorld) for all tool types — eliminates per-tool branching"
  - "Single-object path kept intact with zero behavioural changes"
  - "Light objects handled separately: position updated from pivot formula, direction rotated by rotation portion of localDelta"
  - "PlayerSpawn rotation/scale columns stripped after pivot transform — player spawn only stores position"
metrics:
  duration_minutes: 10
  completed_date: "2026-04-01"
  tasks_completed: 1
  tasks_total: 2
  files_changed: 1
---

# Quick Task 260401-u9o Summary

## One-liner

Multi-selection gizmo using pivot-transform formula: gizmo at centroid, all selected objects transformed simultaneously around that centroid.

## What Was Done

### Task 1: Implement multi-selection gizmo in applyGizmoToSelectedObject

Modified `applyGizmoToSelectedObject` in `EditorViewportInteraction.cpp`:

- Changed guard from `selectedIds.size() != 1` to `selectedIds.empty()` — the only change needed to unlock multi-select.
- Wrapped the entire existing single-object logic inside an `if (selectedIds.size() == 1)` block — zero behavioural change for single selection.
- Added multi-object path after the single-object block:
  1. Computes centroid as the average of `editorSceneObjectAnchor()` for all selected objects.
  2. Places the gizmo matrix at `T(centroid)` (identity rotation, unit scale).
  3. Calls `manipulateEditorGizmo` with this centroid matrix.
  4. Derives `localDelta = T(-centroid) * gizmoMatrix` — the manipulation in centroid-local space.
  5. Applies `newWorld = T(centroid) * localDelta * T(-centroid) * objectWorld` to each selected object.
  6. Special-cases Light objects (update `position` and/or `direction` directly, call `markSceneDirty`).
  7. Special-cases PlayerSpawn (strips rotation/scale columns after pivot transform).

The unified formula handles translate, rotate, and scale uniformly — no per-tool branching required.

Undo/redo is automatic: the existing `gizmoBeforeState`/`gizmoCommand` pattern in `main.cpp` captures the entire document state before the gizmo starts and pushes it when released, covering all selected objects with zero additional changes.

## Commits

| Hash | Description |
|------|-------------|
| dda9b14 | Multi-selection gizmo: translate, rotate, scale all selected objects around centroid |

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Self-Check

- [x] `src/editor/viewport/EditorViewportInteraction.cpp` modified
- [x] Commit `dda9b14` exists
- [x] `cmake --build build --target level-editor` succeeds cleanly

## Self-Check: PASSED
