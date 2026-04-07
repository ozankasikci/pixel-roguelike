---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
plan: "02"
subsystem: door-animation
tags: [refactor, ecs, animation, runtime, bidirectional]
dependency_graph:
  requires: [19-01]
  provides: [tickDoorAnimation, bidirectional-door-animation]
  affects: [DoorAnimationSystem, RuntimeGameSession, LevelBuilder, BehaviorSystem, editor-preview]
tech_stack:
  added: []
  patterns: [free-function extraction for registry-only contexts, bidirectional progress animation]
key_files:
  created: []
  modified:
    - src/game/behavior/DoorAnimationSystem.cpp
    - src/game/behavior/DoorAnimationSystem.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelBuilder.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/runtime/RuntimeGameplay.cpp
    - src/game/runtime/RuntimeGameplay.h
    - src/game/runtime/RuntimeGameSession.cpp
    - src/editor/core/EditorCommand.cpp
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/scene/EditorSceneDocument.h
    - src/editor/ui/inspectors/DoorGroupInspector.cpp
    - src/editor/ui/inspectors/DoorGroupInspector.h
    - src/editor/ui/inspectors/MeshInspector.cpp
    - src/editor/ui/inspectors/SceneSelectionInspector.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - tests/game/test_cathedral_prefabs.cpp
    - tests/game/test_door_group_position.cpp
decisions:
  - "tickDoorAnimation free function extracted from DoorAnimationSystem::update so RuntimeGameSession can call it without an Application& reference"
  - "LevelBuilder::addDoorGroup rewritten to use self-contained LevelDoorPlacement (no child mesh pivot lookup) using hardcoded door_leaf_left mesh with standard hinge geometry"
  - "updateRuntimeDoorAnimation and its duplicate updateRuntimeBehaviors deleted from RuntimeGameplay — animation is now entirely in DoorAnimationSystem"
  - "makePivotLeafModel restored to GameplayPrefabs.cpp (was accidentally deleted in Plan 01)"
  - "LevelMeshPlacement.pivot dead code removed from editor files (field deleted in Plan 01)"
metrics:
  duration: "~60 minutes"
  completed: "2026-04-07"
  tasks_completed: 2
  files_changed: 20
---

# Phase 19 Plan 02: Unify Runtime Door Animation Path Summary

Single animation path via `tickDoorAnimation()` with bidirectional door movement — `updateRuntimeDoorAnimation` deleted and right-leaf bug fixed by design.

## Tasks Completed

| Task | Description | Commit |
|------|-------------|--------|
| 1 | Implement bidirectional DoorAnimationSystem with tickDoorAnimation free function | bd11ec3 |
| 2 | Update BehaviorSystem, LevelBuilder, RuntimeGameSession to use split door components | c101265 |

## What Was Built

**Task 1 — DoorAnimationSystem rewrite:**
- `tickDoorAnimation(registry, deltaTime)` free function: handles PlayerInteractionLock countdown and bidirectional door progress (`+step` for Open, `-step` for Closed)
- `DoorAnimationSystem::update()` delegates to `tickDoorAnimation`
- `DoorAnimationSystem::init()` uses `DoorConfigComponent+DoorStateComponent` view, initializes both leaves to closed state
- `updateDoorLeaf()` uses `makePivotLeafModel` from GameplayPrefabs instead of its own broken local math
- `GameplayPrefabs.cpp`: restored `makePivotLeafModel` and `computeHingeWorldPos` (were deleted in Plan 01); fixed `DoorLeafComponent` field names (`hingePosition`→`basePosition`, `centerOffsetFromHinge`→`pivot`); deleted orphaned `spawnSingleDoor`

**Task 2 — Consumer updates:**
- `LevelBuilder::addDoorGroup` rewritten to use self-contained `LevelDoorPlacement` (no child mesh lookup); emplace `DoorConfigComponent+DoorStateComponent`
- `RuntimeGameplay.cpp/h`: deleted `updateRuntimeDoorAnimation` and the old `updateRuntimeBehaviors` (which used `DoorComponent`)
- `RuntimeGameSession::tick()`: calls `tickDoorAnimation(registry_, deltaTime)` after physics update
- Editor files: renamed `LevelDoorGroupPlacement`→`LevelDoorPlacement` across 8 editor files
- Editor preview: removed dead `LevelMeshPlacement.pivot` pivot-leaf logic; simplified hinge marker to use door group position directly
- Tests: updated `test_cathedral_prefabs` and `test_door_group_position` for new component types

## Deviations from Plan

**1. [Rule 3 - Blocking] makePivotLeafModel missing from GameplayPrefabs.cpp**
- Found during: Task 1
- Issue: Plan 01 accidentally deleted `makePivotLeafModel` from `GameplayPrefabs.cpp` while leaving its declaration in the header. DoorAnimationSystem and LevelBuilder both call it.
- Fix: Restored the full implementation from git history (commit f789e8a)
- Files modified: `src/game/prefabs/GameplayPrefabs.cpp`
- Commit: bd11ec3

**2. [Rule 3 - Blocking] DoorLeafComponent field names broken in GameplayPrefabs.cpp**
- Found during: Task 1
- Issue: Plan 01 updated `DoorLeafComponent.h` to rename `hingePosition`→`basePosition` and `centerOffsetFromHinge`→`pivot`, but `GameplayPrefabs.cpp` still used the old names (two occurrences in `spawnDoorLeaf` and `spawnSingleDoor`)
- Fix: Updated field assignments to use new names; deleted `spawnSingleDoor` (its `SingleDoorSpawnSpec` parameter type was also deleted in Plan 01)
- Files modified: `src/game/prefabs/GameplayPrefabs.cpp`
- Commit: bd11ec3

**3. [Rule 3 - Blocking] LevelDoorGroupPlacement rename incomplete across editor**
- Found during: Task 2
- Issue: Plan 01 renamed `LevelDoorGroupPlacement`→`LevelDoorPlacement` in `LevelDef.h` but did not update 8 editor files that referenced the old name
- Fix: Renamed in all 8 affected files: `EditorSceneDocument.h/.cpp`, `EditorCommand.cpp`, `DoorGroupInspector.h/.cpp`, `SceneSelectionInspector.cpp`, `EditorPreviewWorld.cpp`, `EditorScenePreviewRenderer.cpp`
- Commit: c101265

**4. [Rule 3 - Blocking] LevelMeshPlacement.pivot removed but still accessed in 5 places**
- Found during: Task 2
- Issue: Plan 01 removed `pivot` field from `LevelMeshPlacement` but did not update editor files that used it for pivot-leaf mesh identification
- Fix: Removed all dead pivot-leaf logic from `EditorPreviewWorld.cpp` (two locations), `EditorScenePreviewRenderer.cpp` (hinge visualization), `EditorViewportInteraction.cpp` (gizmo redirect), and `MeshInspector.cpp` (pivot inspector row)
- Commit: c101265

**5. [Rule 3 - Blocking] Test files used deleted DoorComponent and LevelDoorGroupPlacement**
- Found during: Task 2
- Issue: `test_cathedral_prefabs.cpp` referenced `DoorComponent` (deleted in Plan 01); `test_door_group_position.cpp` used `LevelDoorGroupPlacement` and `LevelMeshPlacement.pivot`
- Fix: Updated both tests to use new types
- Commit: c101265

**6. [Rule 3 - Blocking] LevelBuilder.addDoorGroup child-mesh lookup used deleted LevelMeshPlacement.pivot**
- Found during: Task 2
- Issue: `addDoorGroup` searched for child meshes via `pivot.has_value()` — field deleted in Plan 01. Since `LevelDoorPlacement` is now self-contained, rewrote the function to spawn a standard `door_leaf_left` mesh directly.
- Fix: Rewrote `addDoorGroup` to use hardcoded door leaf mesh name and pivot geometry; renamed `LevelDoorGroupPlacement`→`LevelDoorPlacement` in both `LevelBuilder.h` and `LevelBuilder.cpp`
- Commit: c101265

## Known Stubs

None — all animation paths are wired. Door leaves animate via `tickDoorAnimation` which is called from both `DoorAnimationSystem::update` (game) and `RuntimeGameSession::tick` (editor preview).

## Threat Flags

None — no new network endpoints, auth paths, file access, or trust boundary changes introduced.

## Self-Check: PASSED

| Check | Result |
|-------|--------|
| DoorAnimationSystem.h declares tickDoorAnimation | PASS |
| DoorAnimationSystem.cpp implements tickDoorAnimation with bidirectional branches | PASS |
| updateRuntimeDoorAnimation not in src/ | PASS |
| RuntimeGameSession.cpp calls tickDoorAnimation | PASS |
| BehaviorSystem.cpp has DoorTargetState (4 occurrences) | PASS |
| All 30 tests pass | PASS |
| Commit bd11ec3 exists | PASS |
| Commit c101265 exists | PASS |
