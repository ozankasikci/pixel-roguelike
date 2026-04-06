---
phase: 19-refactor-singledoor-into-multi-part-group-architecture-with-
plan: 02
subsystem: editor
tags: [editor, door-group, refactor, inspector, ECS]
dependency_graph:
  requires: [19-01]
  provides: [editor-door-group-support]
  affects: [EditorSceneDocument, EditorPreviewWorld, EditorSelectionSystem, EditorViewportInteraction, EditorCommand, SceneSelectionInspector, MeshInspector]
tech_stack:
  added: []
  patterns: [variant-dispatch, pivot-aware-rendering, click-bubble-to-parent]
key_files:
  created:
    - src/editor/ui/inspectors/DoorGroupInspector.h
    - src/editor/ui/inspectors/DoorGroupInspector.cpp
  modified:
    - src/editor/scene/EditorSceneDocument.h
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/scene/EditorSelectionSystem.cpp
    - src/editor/viewport/EditorViewportInteraction.h
    - src/editor/viewport/EditorViewportInteraction.cpp
    - src/editor/core/EditorCommand.cpp
    - src/editor/ui/inspectors/MeshInspector.cpp
    - src/editor/ui/inspectors/SceneSelectionInspector.cpp
    - src/editor/CMakeLists.txt
    - apps/level_editor/main.cpp
decisions:
  - "DoorGroup entity in EditorPreviewWorld is a minimal logical container (TransformComponent only) — child Mesh objects render themselves, no separate visual for the group root"
  - "Click-bubbles-to-parent implemented in applySelectionHit() with added document parameter — selects DoorGroup when user clicks a child Mesh whose parent is a DoorGroup"
  - "Pivot row in MeshInspector is shown only when placement.pivot.has_value() — zero overhead for non-door meshes"
  - "applySelectionHit signature extended with const EditorSceneDocument& to enable parent lookup without architectural changes"
metrics:
  duration: "~30 minutes"
  completed: "2026-04-07"
  tasks: 2
  files: 11
---

# Phase 19 Plan 02: Editor DoorGroup Refactor Summary

Complete editor-side migration from SingleDoor to DoorGroup: updated enum/variant, added `addDoorGroup()`, wired all switch/visit dispatch sites, added pivot-aware rendering in EditorPreviewWorld, implemented click-bubbles-to-parent selection, created DoorGroupInspector, and added pivot row to MeshInspector.

## Tasks Completed

### Task 1: Update EditorSceneDocument, EditorPreviewWorld, and all switch/visit dispatch sites

Added `DoorGroup` enum value to `EditorSceneObjectKind` and `LevelDoorGroupPlacement` to `EditorSceneObjectPayload` variant. Added `addDoorGroup()` method. Updated all visit/switch sites in:

- `loadFromSceneFile` — iterates `level.doors` and calls `addDoorGroup`
- `supportsParenting` — DoorGroup is a valid parent
- `applyWorldTransform` — extracts `position` and `yawDegrees`
- `toLevelDef` — pushes to `level.doors`
- `localTransformMatrix` — builds matrix from `position` and `yawDegrees`
- `editorSceneObjectKindName` — returns "Door Group"
- `editorSceneObjectLabel` — appends `[name]`
- `EditorPreviewWorld::rebuild()` — creates minimal transform entity; Mesh children with `pivot` get `makePivotLeafModel` override applied
- `EditorPreviewWorld::syncTransforms()` — DoorGroup: updates position/yaw; Mesh: recomputes pivot-aware model if `pivot.has_value()`
- `EditorSelectionSystem::selectionPriority` — DoorGroup at priority 0 (same as Mesh)
- `buildEditorSelectionHandles` — DoorGroup uses bounds AABB or sphere fallback
- `EditorViewportInteraction::isViewportSelectableKind` — DoorGroup always selectable
- `applyGizmoToSelectedObject` — DoorGroup in both single and multi-object gizmo paths
- `applySelectionHit` — click-bubbles-to-parent: if clicked Mesh's parent is DoorGroup, selects the DoorGroup
- `EditorCommand::makeLevelDefFromState` — DoorGroup pushes to `level.doors`

**Commits:** ccf0d20

### Task 2: Create DoorGroupInspector, add pivot row to MeshInspector, delete SingleDoorInspector, build and test

Created `DoorGroupInspector.h` and `DoorGroupInspector.cpp` with full property panel:
- Name (text input)
- Position (DragFloat3), Yaw (DragFloat)
- Open Angle (SliderFloat 0-180), Open Duration (DragFloat)
- Interact Distance, Interact Dot Threshold
- Locked (checkbox), Locked Prompt (conditional text input)

Added optional pivot row to `MeshInspector` (shown when `mesh.pivot.has_value()`).

Updated `SceneSelectionInspector` to dispatch to `drawDoorGroupInspector` for `DoorGroup` kind.

Updated `CMakeLists.txt` to include `DoorGroupInspector.cpp`.

SingleDoorInspector files were already removed in Plan 01.

All 29 tests pass. All three executables (pixel-roguelike, level-editor, and the game runtime) build with zero errors.

**Commits:** 42c1e05

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] applySelectionHit required document parameter for click-bubble logic**
- **Found during:** Task 1 (implementing click-bubbles-to-parent)
- **Issue:** `applySelectionHit` had no access to `EditorSceneDocument` — needed to look up parent object kind
- **Fix:** Added `const EditorSceneDocument& document` parameter to signature in both .h and .cpp; updated two call sites (renderSelectionPicker and apps/level_editor/main.cpp)
- **Files modified:** EditorViewportInteraction.h, EditorViewportInteraction.cpp, apps/level_editor/main.cpp
- **Commit:** ccf0d20

**2. [Rule 3 - Blocking] Worktree was on wrong base commit (d9f4e54 instead of d92a18e)**
- **Found during:** Pre-execution setup
- **Issue:** The worktree was created from an older commit that predated plan 01's changes
- **Fix:** `git reset --soft d92a18eb` to set HEAD, then `git checkout HEAD -- src/ assets/` to restore working tree to plan 01 state
- **Impact:** No code changes required; worktree state corrected before any editing

## Known Stubs

None — all door group properties are fully wired through to the inspector UI and serialize to/from the scene file.

## Self-Check: PASSED

- DoorGroupInspector.h: FOUND
- DoorGroupInspector.cpp: FOUND
- 19-02-SUMMARY.md: FOUND
- Task 1 commit ccf0d20: FOUND
- Task 2 commit 42c1e05: FOUND
- No SingleDoor/LevelSingleDoorPlacement/EditorDoorLeafTag in src/: CLEAN
- All 29 tests pass, all executables build
