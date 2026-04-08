---
phase: quick
plan: 260408-uf2
subsystem: editor
tags: [door, scale, gizmo, editor, bug-fix]
key-files:
  modified:
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
decisions:
  - Clamp door scale to glm::vec3(0.01f) minimum in applyWorldTransform to match existing LevelGroupNode pattern and prevent zero/negative scale
metrics:
  duration: ~5 minutes
  completed: 2026-04-08
  tasks: 1
  files: 2
---

# Quick Task 260408-uf2: Fix Door Scaling in Editor Summary

**One-liner:** Three surgical fixes so door scale flows through localTransformMatrix, applyWorldTransform, and syncTransforms instead of being hardcoded to 1.0.

## What Was Done

Three code paths in the editor ignored the `scale` field already present on `LevelDoorPlacement` (LevelDef.h:116), making gizmo scaling of doors non-functional.

### Fix 1 — EditorSceneDocument.cpp, localTransformMatrix (line 816)

Changed `glm::vec3(1.0f)` to `p.scale` in the LevelDoorPlacement branch so the rendered transform matrix reflects the stored scale.

### Fix 2 — EditorSceneDocument.cpp, applyWorldTransform (line 611)

Added `p.scale = glm::max(scale, glm::vec3(0.01f));` after `p.yawDegrees = rotation.y;` so that gizmo scale edits are written back to the placement struct and persist on save. Matches the LevelGroupNode pattern.

### Fix 3 — EditorPreviewWorld.cpp, syncTransforms, DoorGroup case (line 469)

Added `transform.scale = scale;` so the preview world TransformComponent receives the door scale, making it render at the correct size during editor preview.

## Verification

- Build: clean, no errors or new warnings
- Grep: no remaining `glm::vec3(1.0f)` in LevelDoorPlacement branch of localTransformMatrix
- Grep: `p.scale = glm::max(...)` present in LevelDoorPlacement branch of applyWorldTransform
- Grep: `transform.scale = scale` present in DoorGroup case of syncTransforms

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| 1    | c41d8c8 | Fix door scaling in editor — 3 code paths that hardcoded scale to 1.0 |

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED

- [x] `src/editor/scene/EditorSceneDocument.cpp` modified
- [x] `src/editor/scene/EditorPreviewWorld.cpp` modified
- [x] Commit c41d8c8 exists
