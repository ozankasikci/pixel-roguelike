---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
plan: "04"
subsystem: editor
tags: [door-system, editor-inspector, editor-preview, tests]
dependency_graph:
  requires: [19-02, 19-03]
  provides: [door-inspector-leaf-display, editor-preview-full-door-entities]
  affects: [level-editor, editor-inspector, editor-preview-world]
tech_stack:
  added: []
  patterns: [ImGui raw calls for read-only display sections, addDoorGroup for full entity hierarchy]
key_files:
  created: []
  modified:
    - src/editor/ui/inspectors/DoorGroupInspector.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
decisions:
  - "Leaf mesh section in DoorGroupInspector uses parentNodeId matching rather than pivot field (LevelMeshPlacement has no pivot field in new architecture)"
  - "EditorPreviewWorld passes empty LevelDef to addDoorGroup since the level parameter is unused (self-contained placement)"
  - "Task 3 test files required no changes: Plans 02/03 already updated all test files to new type names"
metrics:
  duration: "~17 minutes"
  completed: "2026-04-07"
  tasks_completed: 3
  files_modified: 2
---

# Phase 19 Plan 04: Editor Components and Tests Summary

DoorGroupInspector extended with leaf mesh display and warning banner. EditorPreviewWorld DoorGroup case updated to spawn full door entity hierarchy via addDoorGroup. All tests verified passing with new type names (already updated by Plans 02/03).

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Extend DoorGroupInspector with leaf mesh display and warning, clean up PrefabInspector and EditorScenePreviewRenderer | c56b7ed | src/editor/ui/inspectors/DoorGroupInspector.cpp |
| 2 | Update EditorPreviewWorld DoorGroup case to spawn full door entities via addDoorGroup | 62d0fd9 | src/editor/scene/EditorPreviewWorld.cpp |
| 3 | Fix all tests to use new type and component names | (no changes needed) | — |

## What Was Built

### DoorGroupInspector leaf mesh section (D-11, D-12, D-13)
- Added a "Leaf Meshes" section below the property table in `DoorGroupInspector.cpp`
- Iterates `document.objects()` to find child meshes whose `parentNodeId == dg.nodeId`
- Displays each leaf mesh name in green with "(leaf)" label
- Shows yellow warning "No leaf meshes found -- this door won't animate." when no children found
- No auto-create buttons (D-13 satisfied)
- Property table is properly closed before the raw ImGui leaf section

### EditorPreviewWorld full door entity spawning (D-14, R2, R5)
- Replaced minimal `TransformComponent`-only entity creation with `builder.addDoorGroup(dg, emptyLevel)` call
- Applies world transform (position, yaw) from document hierarchy before spawning
- `addDoorGroup` creates: leaf entity with `DoorLeafComponent` + `MeshComponent` + `ColliderComponent`, and door root entity with `DoorConfigComponent` + `DoorStateComponent` + `InteractableComponent` + `BehaviorComponent`
- All spawned entities are tracked by the existing `ownerMap_` loop
- `DoorAnimationSystem`'s `view<DoorConfigComponent, DoorStateComponent>` will find these entities during play preview

### PrefabInspector and EditorScenePreviewRenderer (already clean)
- `PrefabInspector.cpp` already had DoubleDoor case removed (done in Plans 02/03)
- `EditorScenePreviewRenderer.cpp` already uses `dg.position` directly (no `computeHingeWorldPos`)

### Tests (already correct)
All 5 target test files (`test_door_group_position.cpp`, `test_level_roundtrip.cpp`, `test_runtime_game_session.cpp`, `test_content_registry.cpp`, `test_cathedral_prefabs.cpp`) were already updated to new type names by Plans 02/03. No changes required.

## Verification

```
cmake --build build    -- all targets built successfully (3 executables + all test targets)
ctest --output-on-failure  -- 30/30 tests pass
grep "No leaf meshes found" DoorGroupInspector.cpp  -- 1 match (warning present)
grep "addDoorGroup" EditorPreviewWorld.cpp  -- match found (full entity spawning)
grep DoubleDoor|LevelDoorGroupPlacement|computeHingeWorldPos src/ tests/  -- 0 matches (clean)
```

## Deviations from Plan

### Auto-fix: Leaf mesh section uses parentNodeId matching without pivot check

**Found during:** Task 1
**Issue:** Plan's suggested code checked `if (!meshPlacement.pivot.has_value()) continue;` but `LevelMeshPlacement` has no `pivot` field in the new architecture (pivot data was removed when door leaves became self-contained via `LevelDoorPlacement`).
**Fix:** Leaf mesh section shows all child meshes by `parentNodeId == dg.nodeId` without pivot check. This is correct for the new unified architecture where pivots live in the door group geometry constants in `LevelBuilder::addDoorGroup`.
**Files modified:** `src/editor/ui/inspectors/DoorGroupInspector.cpp`
**Commit:** c56b7ed

### No-op: Task 3 test files already updated

**Found during:** Task 3 pre-read
**Observation:** All 5 test files were already updated to new type names by Plans 02/03. No `LevelDoorGroupPlacement`, `DoorComponent`, `DoubleDoor`, `spawnDoubleDoor`, or `computeHingeWorldPos` references found. Task 3 completed as verification-only.

### No-op: PrefabInspector and EditorScenePreviewRenderer already clean

**Found during:** Task 1 pre-read
**Observation:** `PrefabInspector.cpp` already had no DoubleDoor case; `EditorScenePreviewRenderer.cpp` already used `dg.position` directly. No changes needed for these files.

## Known Stubs

None. All door functionality is wired through real `addDoorGroup` calls with real entity hierarchies.

## Self-Check: PASSED

- [x] `src/editor/ui/inspectors/DoorGroupInspector.cpp` exists and contains leaf mesh warning
- [x] `src/editor/scene/EditorPreviewWorld.cpp` exists and calls `addDoorGroup`
- [x] Commit c56b7ed exists (Task 1)
- [x] Commit 62d0fd9 exists (Task 2)
- [x] All 30 tests pass
- [x] All 3 executables compile
