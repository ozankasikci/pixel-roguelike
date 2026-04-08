---
phase: quick-260408-q6e
plan: "01"
subsystem: editor/viewport
tags: [gizmo, scale, ux, imguizmo]
one-liner: "Scale gizmo delta amplified 1.5x via deltaMatrix capture and per-axis shift-multiply-shift"
dependency_graph:
  requires: []
  provides: [amplified-scale-gizmo]
  affects: [editor/viewport/EditorViewportController]
tech_stack:
  added: []
  patterns: [delta-amplification via ImGuizmo deltaMatrix]
key_files:
  modified:
    - src/editor/viewport/EditorViewportController.cpp
decisions:
  - "Use shift-multiply-shift formula on scale delta axes (subtract 1, multiply by 1.5, add 1 back) to correctly amplify relative scale change without compounding"
  - "Capture deltaMatrix from ImGuizmo::Manipulate 6th parameter; was previously nullptr"
metrics:
  duration: "5 minutes"
  completed: "2026-04-08"
  tasks_completed: 1
  files_modified: 1
---

# Phase quick-260408-q6e Plan 01: Scale Gizmo Amplification Summary

Scale gizmo delta amplified 1.5x via deltaMatrix capture and per-axis shift-multiply-shift, making drag feel 50% more responsive with no change to Translate or Rotate behavior.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Amplify scale gizmo delta by 1.5x | a3eddd3 | src/editor/viewport/EditorViewportController.cpp |

## What Was Done

Added `constexpr float kScaleAmplification = 1.5f` to the anonymous namespace alongside other sensitivity constants.

In `manipulateEditorGizmo()`:
- Saved `originalMatrix` before the Manipulate call
- Declared `glm::mat4 deltaMatrix(1.0f)` and passed it as the 6th argument to `ImGuizmo::Manipulate` (previously `nullptr`)
- After Manipulate, when `tool == EditorTransformTool::Scale && ImGuizmo::IsUsing()`, extracted the diagonal scale axes from deltaMatrix, applied the shift-multiply-shift formula `(scaleDelta - 1) * 1.5 + 1`, then rebuilt `modelMatrix = glm::scale(originalMatrix, amplified)`
- Translate and Rotate paths have zero changes

## Verification

- `cmake --build build --target level-editor` completed with no warnings or errors
- Only `EditorViewportController.cpp` was modified

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Threat Flags

None — no new network endpoints, auth paths, file access patterns, or schema changes.

## Self-Check: PASSED

- File `src/editor/viewport/EditorViewportController.cpp` exists and contains `kScaleAmplification`
- Commit `a3eddd3` exists in git log
