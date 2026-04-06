---
phase: quick
plan: 260406-tq0
subsystem: editor/scene
tags: [editor, door, gizmo, ecs, bug-fix]
dependency_graph:
  requires: []
  provides: [correct-single-door-gizmo-sync]
  affects: [EditorPreviewWorld, SingleDoor visual preview]
tech_stack:
  added: []
  patterns: [EnTT marker tag component for entity disambiguation]
key_files:
  created: []
  modified:
    - src/editor/scene/EditorPreviewWorld.h
    - src/editor/scene/EditorPreviewWorld.cpp
decisions:
  - Use EditorDoorLeafTag marker component to distinguish frame vs leaf in the shared ownerMap_ key
  - Update mesh.modelOverride directly in syncTransforms (not just TransformComponent) since LevelBuilder::addMesh sets useModelOverride=true and the renderer reads modelOverride
metrics:
  duration: ~15m
  completed: 2026-04-06
  tasks_completed: 1
  files_modified: 2
---

# Phase quick Plan 260406-tq0: Fix Door Gizmo Move Not Applying Transform Summary

**One-liner:** Added `EditorDoorLeafTag` ECS marker so `syncTransforms` can distinguish the door leaf entity from the frame entity and recompute the hinge offset correctly on gizmo move/rotate.

## What Was Done

### Task 1: Add EditorDoorLeafTag and fix SingleDoor sync

**Root cause:** `syncTransforms()` SingleDoor case set `transform.position = position` (root position) for ALL entities owned by a door object. But a SingleDoor spawns two ECS entities during `rebuild()`:
- Frame entity at `rootPosition`
- Leaf entity at `hingeWorldPos` (root + rotated hinge offset: `(-0.45, 0, 0.04)` rotated by `doorYawDegrees`)

Both map to the same owner ID in `ownerMap_`, so `syncTransforms` had no way to distinguish them. Additionally, `LevelBuilder::addMesh` sets `useModelOverride = true` and pre-builds the model matrix — but the old sync code was only updating `TransformComponent.position` while leaving `MeshComponent.modelOverride` stale.

**Fix (3 changes):**

1. **`EditorPreviewWorld.h`** — Added `EditorDoorLeafTag` marker struct before the class declaration
2. **`EditorPreviewWorld.cpp` rebuild()** — Captured the return value of `builder.addMesh` for the leaf and emplace `EditorDoorLeafTag` on it
3. **`EditorPreviewWorld.cpp` syncTransforms()** — Replaced the simple `transform.position = position` with proper hinge offset recomputation:
   - Leaf path: recomputes `hingeWorldPos` from `position` (root) using same rotation math as `rebuild`, updates both `TransformComponent` and `mesh.modelOverride`
   - Frame path: places at root `position`, updates both `TransformComponent` and `mesh.modelOverride`
   - Also added `#include "engine/core/MathUtils.h"` for `makeModelMatrix`

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check

- [x] `src/editor/scene/EditorPreviewWorld.h` modified — `EditorDoorLeafTag` added
- [x] `src/editor/scene/EditorPreviewWorld.cpp` modified — leaf tagging + syncTransforms fix
- [x] Editor target compiles cleanly (`[100%] Built target editor`)
- [x] Commit `5bbc047` exists
