---
phase: 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept
plan: 03
subsystem: game
tags: [cmake, cpp, door, modules, refactor, cutover, cleanup]

# Dependency graph
requires:
  - phase: 21-feature-module-reorganization (plan 02)
    provides: registerDoorModule() fully implemented, keyword registry, action handler registry
provides:
  - registerDoorModule() called at application startup in both runtime and editor
  - LevelLoader uses spawnDoorGroup from DoorSpawner (not builder.addDoorGroup)
  - All old door files deleted from original locations
  - ActionTypes.h includes DoorActionTypes.h for DoorActionParams
  - All three executables build and link cleanly with game_module_door
  - All 13 game tests pass
affects:
  - None (this is the final cleanup plan for phase 21)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CMake executable link pattern: add game_module_door explicitly to pixel-roguelike and level-editor link targets"
    - "Module activation pattern: registerDoorModule() called after content.loadDefaults() at startup"
    - "Include forwarding pattern: ActionTypes.h includes DoorActionTypes.h to expose DoorActionParams in the variant"

key-files:
  created: []
  modified:
    - apps/runtime/main.cpp
    - apps/level_editor/main.cpp
    - apps/runtime/CMakeLists.txt
    - apps/level_editor/CMakeLists.txt
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/ui/inspectors/SceneSelectionInspector.cpp
    - src/editor/CMakeLists.txt
    - src/game/CMakeLists.txt
    - src/game/prefabs/GameplayPrefabs.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/behavior/ActionTypes.h
    - tests/game/CMakeLists.txt
    - tests/game/test_door_group_position.cpp
  deleted:
    - src/game/components/DoorConfigComponent.h
    - src/game/components/DoorStateComponent.h
    - src/game/components/DoorLeafComponent.h
    - src/game/behavior/DoorAnimationSystem.h
    - src/game/behavior/DoorAnimationSystem.cpp
    - src/editor/ui/inspectors/DoorGroupInspector.h
    - src/editor/ui/inspectors/DoorGroupInspector.cpp

key-decisions:
  - "registerDoorModule() placed after content.loadDefaults() in both startup paths — content registry is available, scene loading has not started"
  - "game_module_door added to pixel-roguelike, level-editor, and editor link targets — module is not transitively linked through gameplay"
  - "ActionTypes.h includes DoorActionTypes.h (transitional) to keep DoorActionParams in the ActionParams variant without breaking all callers"
  - "test_level_roundtrip and test_level_def do NOT need registerDoorModule() — neither loads a scene with door_group keywords"
  - "test_door_group_position updated to include DoorMath.h instead of GameplayPrefabs.h for makePivotLeafModel"

requirements-completed: []

# Metrics
duration: ~25min
completed: 2026-04-08
---

# Phase 21 Plan 03: Final Cutover — Door Module as Sole Owner of All Door Code Summary

**All door code consolidated in src/game/modules/door/; old files deleted; registerDoorModule() called at startup; all three executables build; all 13 game tests pass**

## Performance

- **Duration:** ~25 min
- **Completed:** 2026-04-08
- **Tasks:** 2
- **Files modified:** 17 modified, 7 deleted

## Accomplishments

### Task 1: Wire registerDoorModule at startup, update include paths, switch to DoorSpawner

- Added `#include "game/modules/door/DoorModule.h"` and `registerDoorModule()` call to `apps/runtime/main.cpp` after `content.loadDefaults()`
- Added `#include "game/modules/door/DoorModule.h"` and `registerDoorModule()` call to `apps/level_editor/main.cpp` after `content.loadDefaults()`
- Updated `DoorAnimationSystem` include in `apps/runtime/main.cpp` and `RuntimeGameSession.cpp` to module path
- Replaced `DoorConfigComponent.h` + `DoorStateComponent.h` includes in `RuntimeGameSession.cpp` and `EditorPreviewWorld.cpp` with single `DoorComponents.h` module include
- Removed `LevelBuilder::addDoorGroup()` declaration from `LevelBuilder.h` and full implementation from `LevelBuilder.cpp`
- Removed door-specific includes (`DoorConfigComponent.h`, `DoorStateComponent.h`, `DoorLeafComponent.h`, `GameplayPrefabs.h`) from `LevelBuilder.cpp`
- Updated `LevelLoader.cpp` to call `spawnDoorGroup(builder, doorGroup, level)` from `DoorSpawner.h` instead of `builder.addDoorGroup(doorGroup, level)`
- Updated `SceneSelectionInspector.cpp` include to `game/modules/door/editor/DoorGroupInspector.h`
- Added `game_module_door` to `pixel-roguelike`, `level-editor` (app CMakeLists), and `editor` (src/editor/CMakeLists) link targets
- Replaced old `ui/inspectors/DoorGroupInspector.cpp` source in `src/editor/CMakeLists.txt` with module path

### Task 2: Delete old door files, update CMake targets, update tests, verify full build + test pass

- Deleted 7 old door files via `git rm` from components/, behavior/, and editor/ui/inspectors/
- Removed `makePivotLeafModel()` from `GameplayPrefabs.h` and `GameplayPrefabs.cpp` (already in `DoorMath.h/.cpp`)
- Removed inline `struct DoorActionParams` from `ActionTypes.h`; added `#include "game/modules/door/DoorActionTypes.h"` to expose it via the module
- Removed `behavior/DoorAnimationSystem.cpp` from `gameplay` CMake target sources
- Updated `test_door_group_position.cpp` to `#include "game/modules/door/DoorMath.h"` instead of `GameplayPrefabs.h`
- Added `game_module_door` to link libraries for `test_door_group_position`, `test_level_roundtrip`, and `test_runtime_game_session` in tests/game/CMakeLists.txt

## Task Commits

1. **Task 1: Wire registerDoorModule at startup, update include paths, switch to DoorSpawner** - `2425d04`
2. **Task 2: Delete old door files, update CMake, tests** - `9ddb35e`

## Verification

- `cmake --build build` — success, all targets including pixel-roguelike, level-editor, procedural-model-viewer
- `ctest --output-on-failure -L game` — 13/13 tests passed
- `src/game/components/DoorConfigComponent.h` — does NOT exist (deleted)
- `src/game/components/DoorStateComponent.h` — does NOT exist (deleted)
- `src/game/components/DoorLeafComponent.h` — does NOT exist (deleted)
- `src/game/behavior/DoorAnimationSystem.h` — does NOT exist (deleted)
- `src/game/behavior/DoorAnimationSystem.cpp` — does NOT exist (deleted)
- `src/editor/ui/inspectors/DoorGroupInspector.h` — does NOT exist (deleted)
- `src/editor/ui/inspectors/DoorGroupInspector.cpp` — does NOT exist (deleted)
- `src/game/prefabs/GameplayPrefabs.h` — does NOT contain `makePivotLeafModel`
- `src/game/behavior/ActionTypes.h` — does NOT contain `struct DoorActionParams`; contains `#include "game/modules/door/DoorActionTypes.h"`
- `src/game/CMakeLists.txt` — does NOT contain `DoorAnimationSystem.cpp` in gameplay sources
- `src/editor/CMakeLists.txt` — contains `game/modules/door/editor/DoorGroupInspector.cpp`

## Deviations from Plan

**1. [Rule 3 - Blocking] CMake link targets missing from executable CMakeLists**

- **Found during:** Task 1 build verification
- **Issue:** After wiring `registerDoorModule()` and `spawnDoorGroup()`, the linker failed with "Undefined symbols" for both functions. The plan mentioned adding `game_module_door` to executables but did not specify which CMakeLists files to modify. The apps each have their own `CMakeLists.txt` separate from the root.
- **Fix:** Added `game_module_door` to `apps/runtime/CMakeLists.txt`, `apps/level_editor/CMakeLists.txt`, and `src/editor/CMakeLists.txt` (for the `editor` static library which also needs the door types for `EditorPreviewWorld`).
- **Files modified:** `apps/runtime/CMakeLists.txt`, `apps/level_editor/CMakeLists.txt`, `src/editor/CMakeLists.txt`
- **Commit:** `2425d04`

## Known Stubs

None — `registerDoorModule()` is fully called at startup. All door code paths are wired end-to-end.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes introduced. This plan only deleted files and updated include/link paths.

## Self-Check

---

## Self-Check: PASSED

- `src/game/modules/door/DoorModule.h` — EXISTS
- `src/game/modules/door/DoorSpawner.h` — EXISTS
- `src/game/modules/door/DoorMath.h` — EXISTS
- `src/game/modules/door/DoorComponents.h` — EXISTS
- `src/game/modules/door/DoorAnimationSystem.h` — EXISTS
- `src/game/modules/door/editor/DoorGroupInspector.h` — EXISTS
- Commit `2425d04` — EXISTS
- Commit `9ddb35e` — EXISTS
- All 13 game tests pass (verified via ctest output)
- All three executables built successfully (verified via cmake --build output)
