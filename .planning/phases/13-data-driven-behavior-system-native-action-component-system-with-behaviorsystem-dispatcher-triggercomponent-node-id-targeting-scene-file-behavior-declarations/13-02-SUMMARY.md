---
phase: 13-data-driven-behavior-system
plan: 02
subsystem: gameplay
tags: [behavior-system, trigger-system, door-animation, ecs, dispatcher, entt]

requires:
  - phase: 13-01-data-driven-behavior-system
    provides: ActionTypes, BehaviorComponent, TriggerComponent, NodeIndex, NodeIdComponent, LevelDef/LevelBuilder behavior extensions

provides:
  - BehaviorSystem: single-dispatcher for all 14 ActionType cases, reads InteractionFocusState + TriggerComponent flags
  - TriggerSystem: AABB/sphere overlap detection, sets pendingEnter/pendingExit flags only
  - DoorAnimationSystem: pure animation system, no activation logic
  - Refactored CheckpointSystem: visual state only, no InputSystem dependency
  - Centralized action dispatch replacing per-system activation consumption pattern

affects:
  - 13-03-data-driven-behavior-system (scene file behavior wiring)
  - any system that previously consumed activationRequested from InteractionFocusState

tech-stack:
  added: []
  patterns:
    - "BehaviorSystem dispatcher: single switch-on-ActionType pattern handles all 14 action types"
    - "Trigger flag protocol: TriggerSystem sets pendingEnter/pendingExit, BehaviorSystem reads+clears"
    - "Sorted pending action queue: std::lower_bound insertion for delayed action scheduling"
    - "EventBus for fire-and-forget: ShowMessageEvent, PlaySoundEvent, BehaviorEvent published without BehaviorSystem owning handlers"

key-files:
  created:
    - src/game/behavior/BehaviorSystem.h
    - src/game/behavior/BehaviorSystem.cpp
    - src/game/behavior/TriggerSystem.h
    - src/game/behavior/TriggerSystem.cpp
    - src/game/behavior/DoorAnimationSystem.h
    - src/game/behavior/DoorAnimationSystem.cpp
  modified:
    - src/game/systems/CheckpointSystem.h (removed InputSystem& parameter and activation block)
    - src/game/systems/CheckpointSystem.cpp (removed InputSystem dependency)
    - src/game/runtime/RuntimeGameplay.h (removed initializeRuntimeDoors/updateRuntimeDoors declarations)
    - src/game/runtime/RuntimeGameplay.cpp (removed door functions and door helper functions, removed checkpoint activation block)
    - apps/runtime/main.cpp (added BehaviorSystem, TriggerSystem, DoorAnimationSystem; removed DoorSystem)
    - src/game/CMakeLists.txt (added behavior/*.cpp sources, removed systems/DoorSystem.cpp)
  deleted:
    - src/game/systems/DoorSystem.h
    - src/game/systems/DoorSystem.cpp

key-decisions:
  - "BehaviorSystem registered in Gameplay phase AFTER TriggerSystem so pendingEnter/Exit flags are set before dispatch"
  - "DoorAnimationSystem runs AFTER BehaviorSystem in Gameplay phase so opening=true is set before animation tick"
  - "TriggerSystem never dispatches actions — flag-only protocol enforces single dispatch location in BehaviorSystem"
  - "CheckpointSystem drops InputSystem& since activation now flows through BehaviorComponent.onActivate (wired in Plan 03)"
  - "PlayerInteractionLock countdown moved from RuntimeGameplay.updateRuntimeDoors to DoorAnimationSystem.update"
  - "ShowMessageEvent/PlaySoundEvent/BehaviorEvent published on EventBus for decoupled handlers (no coupling to GameOverlays)"

requirements-completed: []

duration: 25min
completed: 2026-04-02
---

# Phase 13 Plan 02: Behavior System Dispatcher and Trigger System Summary

**BehaviorSystem single-dispatcher for all 14 ActionType cases plus TriggerSystem AABB/sphere overlap, replacing per-system activation consumption with centralized action dispatch**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-04-02T09:30:00Z
- **Completed:** 2026-04-02T09:55:00Z
- **Tasks:** 2
- **Files modified:** 11 (3 created, 6 modified, 2 deleted)

## Accomplishments

- BehaviorSystem dispatches all 14 ActionType cases via a single switch, reads InteractionFocusState for E-key activations, consumes TriggerComponent pendingEnter/pendingExit flags, and manages a sorted delayed action queue
- TriggerSystem performs AABB (box) and sphere overlap tests against player position, setting only pendingEnter/pendingExit flags — never dispatching actions directly
- DoorAnimationSystem replaces DoorSystem with pure animation logic (progress, leaf rotation, completion state), with no reference to InteractionFocusState.activationRequested
- CheckpointSystem refactored to visual state only (light intensity, prompt text, feedback timer), InputSystem& constructor parameter removed
- Runtime main.cpp now registers systems in correct order: InteractionSystem (Interaction) -> TriggerSystem (Gameplay) -> BehaviorSystem (Gameplay) -> DoorAnimationSystem (Gameplay)

## Task Commits

1. **Task 1: Implement BehaviorSystem with action dispatch and delayed action queue** - `a01b350` (feat)
2. **Task 2: Implement TriggerSystem, refactor DoorSystem/CheckpointSystem, wire into runtime** - `5b6f358` (feat)

## Files Created/Modified

- `src/game/behavior/BehaviorSystem.h` - BehaviorSystem class with PendingAction struct and event structs (ShowMessageEvent, PlaySoundEvent, BehaviorEvent)
- `src/game/behavior/BehaviorSystem.cpp` - Full dispatch implementation: processNewActivations, processTriggerFlags, processDelayedActions, executeAction (14 cases), schedulePendingAction
- `src/game/behavior/TriggerSystem.h` - TriggerSystem class declaration
- `src/game/behavior/TriggerSystem.cpp` - AABB and sphere overlap detection, sets pendingEnter/pendingExit, supports fireOnce triggers
- `src/game/behavior/DoorAnimationSystem.h` - DoorAnimationSystem class declaration
- `src/game/behavior/DoorAnimationSystem.cpp` - Door animation (progress, leaf yaw, collider updates), PlayerInteractionLock countdown, no activation logic
- `src/game/systems/CheckpointSystem.h` - Removed InputSystem& parameter; default constructor
- `src/game/systems/CheckpointSystem.cpp` - Removed InputSystem dependency and activation block
- `src/game/runtime/RuntimeGameplay.h` - Removed initializeRuntimeDoors/updateRuntimeDoors declarations
- `src/game/runtime/RuntimeGameplay.cpp` - Removed door functions, door helper functions in anonymous namespace, and checkpoint activation block
- `apps/runtime/main.cpp` - Replaced DoorSystem with TriggerSystem+BehaviorSystem+DoorAnimationSystem in Gameplay phase; CheckpointSystem registered without input arg
- `src/game/CMakeLists.txt` - Added behavior/BehaviorSystem.cpp, behavior/DoorAnimationSystem.cpp, behavior/TriggerSystem.cpp; removed systems/DoorSystem.cpp

## Decisions Made

- TriggerSystem flag-only protocol: TriggerSystem sets flags, BehaviorSystem reads + clears. This enforces single dispatch location.
- PlayerInteractionLock countdown moved to DoorAnimationSystem since it's conceptually part of door animation lifecycle
- BehaviorSystem publishes EventBus events (ShowMessageEvent, PlaySoundEvent, BehaviorEvent) rather than directly mutating game state — decouples message display from behavior dispatch

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] RuntimeGameSession.cpp called initializeRuntimeDoors which was removed**
- **Found during:** Task 2 (RuntimeGameplay refactor)
- **Issue:** RuntimeGameSession.cpp had direct calls to initializeRuntimeDoors and updateRuntimeDoors that would fail to compile after removal
- **Fix:** Removed the initializeRuntimeDoors calls from RuntimeGameSession.cpp rebuild() and resetForPlay() methods; removed updateRuntimeDoors call from tick()
- **Files modified:** src/game/runtime/RuntimeGameSession.cpp
- **Verification:** gameplay target compiled cleanly
- **Committed in:** `5b6f358` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Necessary for compilation. RuntimeGameSession is the editor preview session and needed to have the door function calls removed when those functions moved to DoorAnimationSystem.

## Issues Encountered

- Parallel execution: Task 2 files were committed inside a co-running agent's "docs(260402-d7q)" commit due to shared git index. All files are correctly committed and the build is clean.
- The `procedural-model-viewer` target fails to compile due to a pre-existing missing `game/levels/GameAssets.h` include — this predates this plan and is out of scope.

## Known Stubs

None — BehaviorSystem is a functioning dispatcher. Checkpoint activation via BehaviorComponent is not yet wired (that's Plan 03's scope), but the CheckpointSystem visual update path works correctly.

## Next Phase Readiness

- BehaviorSystem, TriggerSystem, DoorAnimationSystem all registered and compiling
- Plan 13-03 can now wire scene file behaviors into BehaviorComponent.onActivate lists to drive door open/close through the new dispatcher
- CheckpointSystem activation wiring via BehaviorComponent is Plan 03 scope

## Self-Check: PASSED

- `src/game/behavior/BehaviorSystem.h` - FOUND
- `src/game/behavior/BehaviorSystem.cpp` - FOUND
- `src/game/behavior/TriggerSystem.h` - FOUND
- `src/game/behavior/TriggerSystem.cpp` - FOUND
- `src/game/behavior/DoorAnimationSystem.h` - FOUND
- `src/game/behavior/DoorAnimationSystem.cpp` - FOUND
- Commit `a01b350` - FOUND
- Commit `5b6f358` - FOUND
- pixel-roguelike builds clean
- level-editor builds clean

---
*Phase: 13-data-driven-behavior-system*
*Completed: 2026-04-02*
