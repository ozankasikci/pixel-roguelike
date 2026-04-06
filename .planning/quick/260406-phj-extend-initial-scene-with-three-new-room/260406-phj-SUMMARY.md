---
phase: quick
plan: 260406-phj
subsystem: game/level
tags: [scene, level-design, doors, rooms, gameplay]
dependency_graph:
  requires: []
  provides: [initial_scene_rooms, interactive_doors]
  affects: [assets/scenes/initial_scene.scene, src/game/scenes/InitialSceneScripted.cpp]
tech_stack:
  added: []
  patterns: [scripted-geometry-registry, spawnSingleDoor-prefab]
key_files:
  created:
    - src/game/scenes/InitialSceneScripted.cpp
  modified:
    - assets/scenes/initial_scene.scene
    - src/game/CMakeLists.txt
decisions:
  - "Used scripted geometry static initializer pattern (same as existing GenericFileScene registry) for zero-friction door registration at startup"
  - "Replaced full west/north/east wall colliders with two partial colliders per wall to keep solid segments while leaving door openings passable"
  - "Door C unlocked (was previously locked with chain padlock) to make all three rooms immediately explorable"
metrics:
  duration: "~10 minutes"
  completed: "2026-04-06T15:37:27Z"
  tasks_completed: 2
  tasks_total: 2
  files_changed: 3
---

# Phase quick Plan 260406-phj: Extend Initial Scene With Three New Rooms Summary

Three interactive doors added to initial_scene with a distinct room behind each one: storage room (west), control office (north), and utility corridor (east), all spawned via the existing spawnSingleDoor() prefab system.

## Tasks Completed

| Task | Name | Commit | Files |
| ---- | ---- | ------ | ----- |
| 1 | Update scene file and create scripted geometry | 0f2184e | assets/scenes/initial_scene.scene, src/game/scenes/InitialSceneScripted.cpp, src/game/CMakeLists.txt |
| 2 | Verify doors and rooms in-game | approved | — (human verification) |

## What Was Built

### Scene Changes (assets/scenes/initial_scene.scene)
- Removed 7 static mesh entries: SM_FrameA, SM_DoorA, SM_FrameD, SM_DoorD, SM_FrameC, SM_DoorC, inst_chain_padlock
- Removed group node_123 (was Door A's transform group)
- Replaced 3 full wall colliders (west/north/east) with 6 partial colliders (2 per wall) flanking the door openings
- Added Room 1 — Storage Room (west, X[-11,-5], Z[-1,3]): floor, ceiling, walls, shelves, cabinet, 2 warm lights
- Added Room 2 — Control Office (north, X[-3,3], Z[6,12]): glossy floor, desk, chair, cabinet, large window, 4 lights
- Added Room 3 — Utility Corridor (east, X[5,13], Z[1.5,4.5]): dark floor, HVAC vents, smoke detector, 4 cool lights

### New File (src/game/scenes/InitialSceneScripted.cpp)
Static initializer calling `GenericFileScene::registerScriptedGeometry("initial_scene", ...)` that spawns:
- Door A: SM_DoorA at (-4.85, 0, 1.0), yaw=90, wooden, 1.2s open
- Door B: SM_DoorD at (0.0, 0, 5.85), yaw=180, heavy metal, 1.8s open (slower/heavier feel)
- Door C: SM_DoorC at (4.85, 0, 3.0), yaw=-90, white, 1.0s open, unlocked

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None. All rooms are fully wired with geometry, colliders, and lighting.

## Self-Check: PASSED
- `src/game/scenes/InitialSceneScripted.cpp` exists: FOUND
- `assets/scenes/initial_scene.scene` updated: FOUND (no SM_DoorA/C/D or SM_FrameA/C/D entries)
- Build succeeded: `[100%] Built target pixel-roguelike`
- Commit 0f2184e exists: FOUND
