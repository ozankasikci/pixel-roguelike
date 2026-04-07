---
phase: 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept
plan: 02
subsystem: game
tags: [cmake, cpp, door, modules, refactor, registration, ecs]

# Dependency graph
requires:
  - phase: 21-feature-module-reorganization (plan 01)
    provides: DoorSerializer.h/.cpp, DoorActionHandler.h/.cpp, DoorModule.h/.cpp stub
provides:
  - LevelDef keyword registry (registerLevelDefKeyword API)
  - BehaviorSystem action handler registry (registerBehaviorActionHandler/findBehaviorActionHandler API)
  - registerDoorModule() fully implemented — wires parseDoorGroup, serializeDoorGroups, handleDoorAction
  - LevelDef.cpp inline door_group parser replaced with keyword dispatch
  - LevelDef.cpp inline door serializer replaced with registered serializer invocation
  - BehaviorSystem.cpp inline door cases replaced with handler registry dispatch
  - RuntimeGameplay.cpp duplicate door action switch eliminated
affects:
  - 21-03 (final cleanup — calls registerDoorModule() from startup, removes old files)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Keyword registry pattern: std::unordered_map<string, handler> singleton for extensible LevelDef parsing"
    - "Action handler registry pattern: std::unordered_map<ActionType, handler> singleton for extensible behavior dispatch"
    - "Module registration pattern: registerDoorModule() wires all three callbacks in one call"

key-files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/behavior/BehaviorSystem.h
    - src/game/behavior/BehaviorSystem.cpp
    - src/game/runtime/RuntimeGameplay.cpp
    - src/game/modules/door/DoorModule.cpp

key-decisions:
  - "Registry singletons use function-local static pattern (Meyer's singleton) — safe for static init order, no global constructor"
  - "BehaviorActionHandler type uses entt::registry& not Application& — keeps door module free of Application dependency"
  - "registerDoorModule() is fully implemented but NOT yet called — Plan 03 calls it from main() startup"
  - "DoorConfigComponent/DoorStateComponent includes removed from BehaviorSystem.cpp and RuntimeGameplay.cpp — door knowledge now in module only"
  - "RuntimeGameplay duplicate door switch fully eliminated — all three action types handled via findBehaviorActionHandler dispatch"

patterns-established:
  - "Keyword registry: LevelDef parses unknown keyword → looks up keywordRegistry() → dispatches to module callback"
  - "Action handler registry: BehaviorSystem/RuntimeGameplay dispatch OpenDoor/CloseDoor/ToggleDoor → findBehaviorActionHandler() → module callback"
  - "Module registration entry point: registerXxxModule() registers all callbacks needed by the module"

requirements-completed: []

# Metrics
duration: ~20min
completed: 2026-04-07
---

# Phase 21 Plan 02: Registration API and Door Module Wiring Summary

**LevelDef and BehaviorSystem now have extensible registration APIs; door module registers its parser, serializer, and action handler callbacks through registerDoorModule(); all inline door code in core systems eliminated**

## Performance

- **Duration:** ~20 min
- **Completed:** 2026-04-07
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

### Task 1: LevelDef keyword registry
- Added `LevelDefParseCallback` and `LevelDefSerializeCallback` type aliases to `LevelDef.h`
- Added `registerLevelDefKeyword()` free function declaration to `LevelDef.h`
- Added `LevelDefKeywordHandler` struct and `keywordRegistry()` Meyer's singleton inside the anonymous namespace of `LevelDef.cpp`
- Added `registerLevelDefKeyword()` implementation outside the anonymous namespace
- Replaced 47-line inline `door_group` parser block with 8-line generic `keywordRegistry().find(kind)` dispatch
- Replaced 14-line inline door serializer loop with 6-line registered serializer invocation loop

### Task 2: BehaviorSystem action handler registry
- Added `BehaviorActionHandler` type alias, `registerBehaviorActionHandler()`, and `findBehaviorActionHandler()` declarations to `BehaviorSystem.h`
- Added `actionHandlerRegistry()` Meyer's singleton and implementations of both registration functions to `BehaviorSystem.cpp`
- Replaced three inline door cases (OpenDoor/CloseDoor/ToggleDoor, ~75 lines) in `BehaviorSystem::executeAction` with a single 6-line handler lookup dispatch
- Removed `DoorConfigComponent.h` and `DoorStateComponent.h` includes from `BehaviorSystem.cpp`
- Replaced entire inline door switch in `RuntimeGameplay::updateRuntimeBehaviors` (~55 lines) with a 4-line `findBehaviorActionHandler` dispatch
- Removed `DoorConfigComponent.h` and `DoorStateComponent.h` includes from `RuntimeGameplay.cpp`; added `BehaviorSystem.h`
- Implemented `registerDoorModule()` to wire `parseDoorGroup`, `serializeDoorGroups`, and `handleDoorAction` for all three action types

## Task Commits

1. **Task 1: LevelDef keyword registry** - `bf2276c`
2. **Task 2: BehaviorSystem registry and DoorModule wiring** - `9df922c`

## Files Modified

- `src/game/level/LevelDef.h` — Added `LevelDefParseCallback`, `LevelDefSerializeCallback`, `registerLevelDefKeyword()` declarations
- `src/game/level/LevelDef.cpp` — Added `keywordRegistry()`, `registerLevelDefKeyword()` impl; replaced inline door_group parser and serializer with dispatch
- `src/game/behavior/BehaviorSystem.h` — Added `BehaviorActionHandler`, `registerBehaviorActionHandler()`, `findBehaviorActionHandler()` declarations; added `ActionTypes.h` include
- `src/game/behavior/BehaviorSystem.cpp` — Added `actionHandlerRegistry()`, registration functions; replaced inline door cases with handler dispatch; removed door component includes
- `src/game/runtime/RuntimeGameplay.cpp` — Replaced inline door switch with handler dispatch; removed door component includes; added `BehaviorSystem.h` include
- `src/game/modules/door/DoorModule.cpp` — Implemented full `registerDoorModule()` wiring all three callbacks

## Deviations from Plan

None — plan executed exactly as written. Both `gameplay` and `game_module_door` targets compiled cleanly after each task.

## Verification

- `cmake --build build --target gameplay` — success, no errors
- `cmake --build build --target game_module_door` — success, no errors
- `LevelDef.cpp` no longer contains inline `door_group` parser or serializer
- `BehaviorSystem.cpp` no longer contains `DoorConfigComponent`/`DoorStateComponent` access
- `RuntimeGameplay.cpp` `updateRuntimeBehaviors` no longer contains door switch cases
- `DoorModule.cpp` registers all three callbacks

## Known Stubs

None — `registerDoorModule()` is fully implemented. It is intentionally not called from any startup path yet; Plan 03 performs that final wiring.

## Next Phase Readiness

- Plan 03 can call `registerDoorModule()` from main() startup (or RuntimeGameSession init) to activate the module system end-to-end
- All registration infrastructure is in place; old inline code has been removed from core systems
- Old door source files (behavior/DoorAnimationSystem.*, prefabs/GameplayPrefabs.*, etc.) still present — Plan 03 removes them

---
*Phase: 21-feature-module-reorganization*
*Completed: 2026-04-07*
