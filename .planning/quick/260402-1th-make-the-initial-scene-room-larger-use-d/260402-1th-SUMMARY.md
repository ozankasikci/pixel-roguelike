---
phase: quick
plan: 260402-1th
subsystem: scene
tags: [scene, doors, prefabs, QuestDoorsPack, interactive, level-design]

requires:
  - phase: quick/260402-1l5
    provides: QDP door scale measurements (0.21 uniform scale for 2.1m opening)

provides:
  - spawnSingleDoor() prefab function with locked/unlocked modes
  - SingleDoorSpawnSpec struct for single-leaf door configuration
  - initial_scene.scene expanded to 10x8m
  - Interactive Door A (west, SM_DoorA) and Door B (north, SM_DoorD) via E key
  - Locked Door C (east, SM_DoorC) with chain/padlock visual and locked prompt

affects: [initial_scene, GenericFileScene, GameplayPrefabs, door-system]

tech-stack:
  added: []
  patterns:
    - spawnSingleDoor prefab pattern for single-leaf QDP doors (hinge-offset positioning)
    - Scripted geometry registered per-scene-id in static initializer

key-files:
  created: []
  modified:
    - assets/scenes/initial_scene.scene
    - src/game/prefabs/GameplayPrefabData.h
    - src/game/prefabs/GameplayPrefabs.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/scenes/GenericFileScene.cpp

key-decisions:
  - "QDP door hinge placed at local (-0.45, 0, 0.04) offset from rootPosition, rotated by doorYawDegrees"
  - "centerOffsetFromHinge = (0.42, 0, 0) in hinge-local space (half of scaled ~0.85m door width)"
  - "Locked doors get only InteractableComponent (no DoorComponent) so DoorSystem cannot open them"
  - "Frame meshes (SM_FrameA/C/D) placed at 0.21 scale co-located with door rootPosition"
  - "initial_scene bounds expanded to X[-5,5] Y[0,4] Z[-2,6] — 10m wide 8m deep 4m tall"

requirements-completed: []

duration: 15min
completed: 2026-04-02
---

# Quick Task 260402-1th Summary

**initial_scene expanded from 6x4m to 10x8m with spawnSingleDoor prefab, QDP frames on all three openings, and E-key interaction for Door A (wooden) and Door B (metal); Door C locked with chain/padlock**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-04-02
- **Completed:** 2026-04-02
- **Tasks:** 2 of 3 (Task 3 is human-verify checkpoint)
- **Files modified:** 5

## Accomplishments

- Added `SingleDoorSpawnSpec` struct and `spawnSingleDoor()` prefab function using hinge-offset positioning for QDP single-leaf doors
- Expanded `initial_scene.scene` from 6x4m (X[-3,3] Z[0,4]) to 10x8m (X[-5,5] Z[-2,6]) with 5x4 floor/ceiling tile grids, 4-light layout, and adjusted colliders
- Registered `buildInitialSceneGeometry()` for the "initial_scene" level ID; spawns 3 QDP doors (Door A openable, Door B openable, Door C locked) with chain/padlock mesh detail

## Task Commits

1. **Task 1: Add spawnSingleDoor prefab and expand room geometry** - `021c266` (feat)
2. **Task 2: Register initial_scene scripted geometry with interactive QDP doors** - `196510d` (feat)

## Files Created/Modified

- `assets/scenes/initial_scene.scene` - Expanded to 10x8m; QDP door mesh entries removed (moved to scripted geometry)
- `src/game/prefabs/GameplayPrefabData.h` - Added `SingleDoorSpawnSpec` struct
- `src/game/prefabs/GameplayPrefabs.h` - Added `spawnSingleDoor` declaration
- `src/game/prefabs/GameplayPrefabs.cpp` - Implemented `spawnSingleDoor` with hinge calc, DoorLeafComponent, StaticColliderComponent, locked/unlock branching
- `src/game/scenes/GenericFileScene.cpp` - Added `buildInitialSceneGeometry()`, registered for "initial_scene"

## Decisions Made

- Hinge world position computed by rotating local offset (-0.45, 0, 0.04) by `doorYawDegrees` using explicit cos/sin rather than GLM rotation matrix to avoid header dependency in this file.
- `centerOffsetFromHinge` is stored as (0.42, 0, 0) in hinge-local space — the `makeDoorLeafModel()` function in RuntimeGameplay rotates this offset by current yaw at runtime to produce world-space center.
- Locked doors omit `DoorComponent` entirely; only `InteractableComponent` is added. DoorSystem only processes entities with `DoorComponent`, so locked doors never animate.
- Frame mesh placed at `rootPosition` with same `doorYawDegrees` yaw and scale 0.21 — matches QDP Unity prefab origin convention.

## Deviations from Plan

None — plan executed exactly as written. Both spawnSingleDoor implementation and scene expansion followed plan specifications.

## Issues Encountered

None. Build passed cleanly on both tasks.

## Checkpoint: Human Verification Required

Task 3 is a `checkpoint:human-verify`. To verify:

1. Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && ./build/pixel-roguelike`
2. Verify the room is noticeably larger (10x8m vs old 6x4m)
3. Walk to Door A (west wall) — press E to open; should swing 80 degrees
4. Walk to Door B (north wall) — press E to open; metal door, 1.5s animation
5. Walk to Door C (east wall) — press E; should show "This door is chained shut", door stays closed
6. Check door frames (SM_FrameA/C/D) align with wall openings
7. Check 4-light ceiling coverage across the larger space

Type "approved" or describe issues to fix.

## Self-Check

- `assets/scenes/initial_scene.scene` — expanded 10x8m scene file
- `src/game/prefabs/GameplayPrefabData.h` — SingleDoorSpawnSpec struct
- `src/game/prefabs/GameplayPrefabs.h` — spawnSingleDoor declaration
- `src/game/prefabs/GameplayPrefabs.cpp` — spawnSingleDoor implementation
- `src/game/scenes/GenericFileScene.cpp` — initial_scene geometry registration

## Self-Check: PASSED

All files confirmed present. Commits 021c266 and 196510d verified in git log.
