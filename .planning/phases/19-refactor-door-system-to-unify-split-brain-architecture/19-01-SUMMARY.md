---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
plan: "01"
subsystem: door-components
tags: [refactor, ecs, components, level-def]
dependency_graph:
  requires: []
  provides: [DoorConfigComponent, DoorStateComponent, LevelDoorPlacement]
  affects: [DoorAnimationSystem, BehaviorSystem, GameplayPrefabs, RuntimeGameSession, LevelDef]
tech_stack:
  added: []
  patterns: [ECS POD components with free-function helpers, split config/state components]
key_files:
  created:
    - src/game/components/DoorConfigComponent.h
    - src/game/components/DoorStateComponent.h
  modified:
    - src/game/components/DoorComponent.h (deleted)
    - src/game/behavior/DoorAnimationSystem.cpp
    - src/game/behavior/BehaviorSystem.cpp
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
decisions:
  - "DoorStateComponent uses free-function helpers (isDoorFullyClosed, isDoorFullyOpen, isDoorMoving) rather than member methods to comply with POD component rule"
  - "restoreBaselineState deferred makePivotLeafModel leaf reset to Plan 02 (function not yet available in this plan)"
  - "pivot field removed from LevelMeshPlacement (was a door-specific hack, superseded by DoorLeafComponent)"
metrics:
  duration: "~30 minutes"
  completed: "2026-04-07"
  tasks_completed: 2
  files_changed: 9
---

# Phase 19 Plan 01: Create DoorConfigComponent/DoorStateComponent and Rename LevelDoorPlacement Summary

Split `DoorComponent` into `DoorConfigComponent` (spawn-time config) + `DoorStateComponent` (runtime state with `DoorTargetState` enum), and renamed `LevelDoorGroupPlacement` to `LevelDoorPlacement` throughout the codebase.

## Tasks Completed

| Task | Description | Commit |
|------|-------------|--------|
| 1 | Create DoorConfigComponent.h + DoorStateComponent.h, delete DoorComponent.h, update all consumer includes and minimal logic | f789e8a |
| 2 | Add LevelDoorPlacement struct to LevelDef, restore door_group parser/serializer/resolver with new type name | dfef58b |

## What Was Built

**Task 1 — Component split:**
- `DoorConfigComponent`: holds `leftLeaf`, `rightLeaf`, `interactDistance`, `interactDotThreshold`, `openDuration`, `openAngle`, `locked`, `lockedPrompt`
- `DoorStateComponent`: holds `progress` and `DoorTargetState targetState` (Closed/Open); free functions `isDoorFullyClosed`, `isDoorFullyOpen`, `isDoorMoving` defined in header (no member methods — POD compliance)
- `DoorComponent.h` deleted
- `DoorAnimationSystem.cpp`: updated view to `DoorConfigComponent, DoorStateComponent`; animation loop uses new state fields
- `BehaviorSystem.cpp`: OpenDoor/CloseDoor/ToggleDoor actions use split components; `door->opening`/`door->opened` replaced with `targetState` + free functions
- `GameplayPrefabs.cpp`: both `spawnDoubleDoor` and `spawnSingleDoor` emplace `DoorConfigComponent` + `DoorStateComponent`
- `RuntimeGameSession.cpp`: `RuntimeMutableSnapshot::doors` stores `DoorStateComponent`; capture/restore updated accordingly

**Task 2 — LevelDef rename:**
- `LevelDoorGroupPlacement` renamed to `LevelDoorPlacement` (all fields unchanged)
- `LevelDef::doors` field type updated to `std::vector<LevelDoorPlacement>`
- Parser block restored with `LevelDoorPlacement dg;` using `door_group` keyword (unchanged)
- Serializer restored outputting `door_group` keyword (unchanged)
- `resolveLevelHierarchy`: `LevelNodeRef::Kind::DoorGroup` enum value added; `localMatrixFor` and resolve-and-write-back cases added

## Deviations from Plan

**1. [Rule 1 - Minimal scope] restoreBaselineState: skipped makePivotLeafModel leaf reset**
- Found during: Task 1
- Issue: Plan specified `restoreBaselineState` should call `makePivotLeafModel` to reset leaf visual state, but that function is not declared in the worktree at this point (it lives in GameplayPrefabs which was restructured in pre-existing working tree changes)
- Fix: Implemented the DoorStateComponent patch only; leaf visual reset deferred to Plan 02/03 where the full animation system is wired
- Files modified: `src/game/runtime/RuntimeGameSession.cpp`

**2. [Observation] pivot field removed from LevelMeshPlacement**
- The `pivot` field (`std::optional<glm::vec3>`) was already removed from `LevelMeshPlacement` in the pre-existing working tree changes. Task 2 preserved this removal. The field was a door-specific hack that is superseded by `DoorLeafComponent.hingePosition`.

## Known Stubs

None — this plan is purely a type/structure refactor. No UI rendering paths, no data flows that would produce empty/placeholder output.

## Self-Check: PASSED

| Check | Result |
|-------|--------|
| DoorConfigComponent.h exists | PASS |
| DoorStateComponent.h exists | PASS |
| DoorComponent.h deleted | PASS |
| 19-01-SUMMARY.md exists | PASS |
| Commit f789e8a exists | PASS |
| Commit dfef58b exists | PASS |
