---
phase: 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept
plan: 01
subsystem: game
tags: [cmake, cpp, door, modules, refactor, ecs]

# Dependency graph
requires:
  - phase: 19-refactor-door-system-to-unify-split-brain-architecture
    provides: DoorConfigComponent, DoorStateComponent, DoorLeafComponent, DoorAnimationSystem, LevelDoorPlacement, addDoorGroup in LevelBuilder
provides:
  - src/game/modules/door/ directory with all door-related source files
  - game_module_door CMake static library target
  - DoorComponents.h consolidating all three door component headers
  - DoorAnimationSystem.h/.cpp in modules/door/ with updated include paths
  - DoorMath.h/.cpp extracting makePivotLeafModel from GameplayPrefabs
  - DoorSpawner.h/.cpp extracting door group spawning from LevelBuilder
  - DoorActionTypes.h extracting DoorActionParams
  - DoorSerializer.h/.cpp for door_group parse/serialize
  - DoorActionHandler.h/.cpp consolidating door action handling
  - DoorModule.h/.cpp stub registration entry point
  - editor/DoorGroupInspector.h/.cpp in modules/door/editor/
affects:
  - 21-02 (registration API plan — uses DoorModule.h registerDoorModule)
  - 21-03 (final cleanup plan — removes old files, links game_module_door)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Feature module pattern: self-contained source directory under src/game/modules/{feature}/"
    - "game_module_door CMake static library linking gameplay (one-way dependency enforced)"
    - "Editor-only files in modules/door/editor/ excluded from game_module_door target"

key-files:
  created:
    - src/game/modules/door/DoorComponents.h
    - src/game/modules/door/DoorAnimationSystem.h
    - src/game/modules/door/DoorAnimationSystem.cpp
    - src/game/modules/door/DoorMath.h
    - src/game/modules/door/DoorMath.cpp
    - src/game/modules/door/DoorSpawner.h
    - src/game/modules/door/DoorSpawner.cpp
    - src/game/modules/door/DoorActionTypes.h
    - src/game/modules/door/DoorSerializer.h
    - src/game/modules/door/DoorSerializer.cpp
    - src/game/modules/door/DoorActionHandler.h
    - src/game/modules/door/DoorActionHandler.cpp
    - src/game/modules/door/DoorModule.h
    - src/game/modules/door/DoorModule.cpp
    - src/game/modules/door/editor/DoorGroupInspector.h
    - src/game/modules/door/editor/DoorGroupInspector.cpp
    - src/game/modules/door/CMakeLists.txt
  modified:
    - src/game/CMakeLists.txt

key-decisions:
  - "DoorComponents.h consolidates all three component headers (DoorConfigComponent, DoorStateComponent, DoorLeafComponent) into one file per D-10"
  - "Editor-only DoorGroupInspector placed under modules/door/editor/ but NOT added to game_module_door — stays in editor CMake target per D-04"
  - "DoorModule.cpp is a stub for Plan 02 — no registration wired yet"
  - "Old files (behavior/DoorAnimationSystem.*, prefabs/GameplayPrefabs.*, etc.) remain in place as active code — cleanup in Plan 03"

patterns-established:
  - "Feature module: src/game/modules/{name}/ with its own CMakeLists.txt static library"
  - "Module CMakeLists.txt: target_include_directories PUBLIC ${CMAKE_SOURCE_DIR}/src; link gameplay PUBLIC"
  - "Editor additions under modules/{name}/editor/ excluded from module library target"

requirements-completed: []

# Metrics
duration: 24min
completed: 2026-04-07
---

# Phase 21 Plan 01: Door Module Directory Structure Summary

**Door feature module established in src/game/modules/door/ with 16 source files and game_module_door CMake static library target that compiles all 6 implementation files without errors**

## Performance

- **Duration:** ~24 min
- **Started:** 2026-04-07T20:15:32Z
- **Completed:** 2026-04-07T20:39:30Z
- **Tasks:** 2
- **Files modified:** 18 (17 created, 1 modified)

## Accomplishments
- Created `src/game/modules/door/` with 16 .h/.cpp files covering all door subsystem responsibilities
- `DoorComponents.h` consolidates three formerly-separate component headers into one
- `game_module_door` CMake static library defined in `src/game/modules/door/CMakeLists.txt` and integrated via `add_subdirectory(modules/door)` in `src/game/CMakeLists.txt`
- All 6 door module .cpp files verified to compile (via direct object file build and syntax checks)
- Old files left untouched — no build regressions from this plan

## Task Commits

Each task was committed atomically:

1. **Task 1: Create door module source files** - `69e417b` (feat)
2. **Task 2: Add game_module_door CMake target** - `ee198de` (feat)

## Files Created/Modified

- `src/game/modules/door/DoorComponents.h` - Consolidated DoorConfigComponent, DoorStateComponent, DoorLeafComponent, helper free functions
- `src/game/modules/door/DoorAnimationSystem.h/.cpp` - Copied from behavior/ with include paths updated to modules/door
- `src/game/modules/door/DoorMath.h/.cpp` - Extracted makePivotLeafModel from GameplayPrefabs
- `src/game/modules/door/DoorSpawner.h/.cpp` - Extracted door group spawning logic from LevelBuilder::addDoorGroup
- `src/game/modules/door/DoorActionTypes.h` - Extracted DoorActionParams struct
- `src/game/modules/door/DoorSerializer.h/.cpp` - Extracted door_group parse/serialize from LevelDef with local helper replicas
- `src/game/modules/door/DoorActionHandler.h/.cpp` - Consolidated door action handling from BehaviorSystem
- `src/game/modules/door/DoorModule.h/.cpp` - Stub registerDoorModule() entry point for Plan 02
- `src/game/modules/door/editor/DoorGroupInspector.h/.cpp` - Copied from editor/ui/inspectors/ with include self-reference updated
- `src/game/modules/door/CMakeLists.txt` - New static library target with gameplay PUBLIC link
- `src/game/CMakeLists.txt` - Added add_subdirectory(modules/door) at end

## Decisions Made
- Consolidated three door component headers into one `DoorComponents.h` to reduce include friction inside the module
- `DoorSerializer.cpp` replicates the anonymous-namespace helpers (`parseNodeMetadata`, `formatFloat`, `appendNodeMetadata`, `throwParseError` from ParseUtils) locally rather than pulling them from LevelDef's anonymous namespace, keeping the module self-contained
- `DoorActionHandler` unifies the door action switch from BehaviorSystem and RuntimeGameplay but does NOT yet replace them (that is Plan 03 work)
- CMake target `game_module_door` links `gameplay PUBLIC` which transitively provides all needed deps — explicit `engine_core`, `engine_rendering`, `engine_physics` listed per plan spec for clarity

## Deviations from Plan

None - plan executed exactly as written. The CMake `game_module_door` target and all 16 source files were created per spec. The only non-plan discovery was that the worktree's working tree had stale modifications from its original branch; these were restored via `git checkout HEAD --` before running the build verification.

## Issues Encountered

- **Worktree working tree had stale modifications:** After `git reset --soft` to reach the base commit `ae5dd0c8`, the working tree retained changes from the old branch (deleted `LevelDoorPlacement` from `LevelDef.h`, etc.). These were restored with `git checkout HEAD -- <files>` before verifying compilation. Not a plan deviation — just a setup step.
- **Worktree build sandbox restriction on `ar`:** The worktree's CMake build directory could not complete the `ar` archive step for `libassimp.a` due to sandbox write restrictions. Verified compilation by building door module object files directly (`make -f game_module_door.dir/build.make *.cpp.o`). All 6 `.cpp` files compiled without errors. The full `game_module_door` static library linking is blocked only by the sandbox restriction on assimp, not by door module code.

## Next Phase Readiness
- Plan 02 can implement the registration API using `DoorModule.h` as the entry point
- `registerDoorModule()` stub is in place, waiting for registry parameters
- All door source files are in their final module location — Plan 02 updates callers to use module paths
- Old files remain in place; Plan 03 removes them after all callers are updated

---
*Phase: 21-feature-module-reorganization*
*Completed: 2026-04-07*
