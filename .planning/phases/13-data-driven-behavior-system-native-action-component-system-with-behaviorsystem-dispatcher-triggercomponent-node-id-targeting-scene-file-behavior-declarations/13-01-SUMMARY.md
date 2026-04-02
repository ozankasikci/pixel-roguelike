---
phase: 13-data-driven-behavior-system
plan: 01
subsystem: behavior-foundation
tags: [behavior, ecs, scene-parser, level-loading]
dependency_graph:
  requires: []
  provides:
    - ActionType enum and typed ActionParams variant (14 action types)
    - BehaviorComponent POD struct with four event lists
    - TriggerComponent with Box/Sphere shapes and pendingEnter/pendingExit
    - NodeIdComponent for entity node ID storage
    - NodeIndex for entity resolution by name
    - Scene parser sub-line support for behavior and interactable declarations
    - LevelBuilder behavior/trigger/nodeId attachment methods
    - LevelLoader NodeIndex construction from registry.view<NodeIdComponent>()
  affects:
    - LevelDef.h (new structs added)
    - LevelDef.cpp (parser refactored)
    - LevelBuilder.h/cpp (new public methods)
    - LevelLoader.cpp (entity capture, NodeIndex build)
tech_stack:
  added: []
  patterns:
    - std::variant for typed action parameter blocks (ActionParams)
    - registry.ctx() for NodeIndex storage (accessible to BehaviorSystem in Plan 02)
    - Line-buffer parser with indented sub-line detection
key_files:
  created:
    - src/game/behavior/ActionTypes.h
    - src/game/behavior/BehaviorComponent.h
    - src/game/behavior/TriggerComponent.h
    - src/game/behavior/NodeIdComponent.h
    - src/game/behavior/NodeIndex.h
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
decisions:
  - NodeIndex built via registry.view<NodeIdComponent>() after all entity placement, not from parallel vectors — ensures scripted geometry entities are also indexed
  - Parser refactored to buffer all lines first, enabling look-ahead for indented sub-lines without backtracking
  - attachNodeId uses emplace_or_replace so scripted geometry entities can receive a node ID without double-emplace errors
  - LevelBuilder.h includes LevelDef.h directly so TriggerPlacement/BehaviorDeclaration types are available in method signatures without forward-declaring
metrics:
  duration: 15 minutes
  completed_date: 2026-04-02
  tasks_completed: 3
  files_changed: 10
---

# Phase 13 Plan 01: Behavior System Type Foundation Summary

**One-liner:** ActionType enum (14 types) + typed ActionParams variant + BehaviorComponent/TriggerComponent/NodeIndex components + scene parser sub-line support + LevelLoader NodeIndex build.

## What Was Built

This plan establishes the complete data-layer foundation for the behavior system without implementing runtime dispatch (that is Plan 02). The contracts, component types, and data pipeline are now in place.

### Task 1: Behavior Type Definitions (commit: 68449e9)

Created `src/game/behavior/` directory with five header-only files:

- **ActionTypes.h** — `enum class ActionType : uint8_t` with 14 values (OpenDoor through TeleportPlayer), typed `ActionParams = std::variant<...>` with 10 parameter struct variants, `ActionEntry` POD struct with type/target/delay/fireOnce/params/fired fields.
- **BehaviorComponent.h** — `BehaviorComponent` with four `std::vector<ActionEntry>` event lists (onActivate/onEnter/onExit/onTimer) and enabled flag.
- **TriggerComponent.h** — `enum class TriggerShape : uint8_t { Box, Sphere }` + `TriggerComponent` with halfExtents (box), radius (sphere), fireOnce, playerInside (runtime), pendingEnter/pendingExit (TriggerSystem sets, BehaviorSystem consumes).
- **NodeIdComponent.h** — `NodeIdComponent { std::string nodeId }` POD, emplaced by LevelBuilder on every entity with a non-empty node ID.
- **NodeIndex.h** — `NodeIndex { unordered_map<string, entity> byNodeId }` with `add()`, single-arg `resolve()`, and two-arg `resolve(nodeId, self)` that maps "self"/empty to the caller entity.

### Task 2: Scene Parser Extension (commit: bfc9804)

Extended `LevelDef.h` with:
- `BehaviorDeclaration { eventType, ActionEntry }` — carries a single behavior sub-line parsed from the scene file.
- `InteractableDeclaration { promptText, distance, dotThreshold }` — carries interactable sub-line data.
- `TriggerPlacement` — top-level trigger entity with shape, position, halfExtents/radius, nodeId, fireOnce, and behavior declarations.
- `LevelMeshPlacement` gains `behaviors` and `interactable` fields.
- `LevelLightPlacement` gains `behaviors` field.
- `LevelDef` gains `triggers` vector.

Refactored `LevelDef.cpp` parser to buffer all lines and detect indented sub-lines (lines starting with space or tab). Sub-lines are routed to `attachSubLine()` which dispatches on keyword: `on_activate`, `on_enter`, `on_exit`, `on_timer`, `behavior`, `interactable`. Added `parseActionEntry()` helper mapping string action names to `ActionType` enum and parsing keyword tokens into the appropriate `ActionParams` variant. Added `trigger_box` and `trigger_sphere` top-level entity parsers. Serializer emits 2-space indented sub-lines for interactable and behavior declarations, enabling round-trip.

### Task 3: LevelBuilder and LevelLoader Wiring (commit: 311f2e7)

Added four new methods to `LevelBuilder`:
- `attachNodeId(entity, nodeId)` — emplaces `NodeIdComponent` if nodeId is non-empty.
- `attachBehaviors(entity, declarations)` — creates/extends `BehaviorComponent`, routing each declaration to the correct event list by eventType string.
- `attachInteractable(entity, decl)` — creates/extends `InteractableComponent` with promptText, distance, dotThreshold.
- `addTrigger(placement)` — creates transform entity, emplaces `TriggerComponent`, calls attachNodeId and attachBehaviors.

Updated `LevelLoader::load()`:
- Captures entity from `addMesh()` and calls `attachNodeId`/`attachBehaviors`/`attachInteractable`.
- Captures entity from `addLight()` and calls `attachNodeId`/`attachBehaviors`.
- Adds trigger placement loop calling `builder.addTrigger()`.
- After all placement loops (including scripted geometry which runs before the placement loops), builds `NodeIndex` via `registry.view<NodeIdComponent>()` and stores in `registry.ctx()`.

## Verification

1. All five behavior headers exist in `src/game/behavior/` with correct POD struct patterns.
2. `cmake --build . --target gameplay` compiles cleanly (verified).
3. `cmake --build . --target level-editor` compiles cleanly (verified — no regressions in editor layer).
4. Scene snippet with indented sub-lines will parse and re-serialize correctly (parser tested structurally).

## Deviations from Plan

None — plan executed exactly as written.

The plan noted "No CMake changes needed for Plan 01 Task 1" — confirmed. The gameplay target already exposes `src/` as a public include directory, so all five behavior headers are discoverable without CMake changes.

## Self-Check: PASSED

**Files exist:**
- src/game/behavior/ActionTypes.h — FOUND
- src/game/behavior/BehaviorComponent.h — FOUND
- src/game/behavior/TriggerComponent.h — FOUND
- src/game/behavior/NodeIdComponent.h — FOUND
- src/game/behavior/NodeIndex.h — FOUND
- src/game/level/LevelDef.h — FOUND (modified)
- src/game/level/LevelDef.cpp — FOUND (modified)
- src/game/level/LevelBuilder.h — FOUND (modified)
- src/game/level/LevelBuilder.cpp — FOUND (modified)
- src/game/level/LevelLoader.cpp — FOUND (modified)

**Commits exist:**
- 68449e9 — FOUND (Task 1)
- bfc9804 — FOUND (Task 2)
- 311f2e7 — FOUND (Task 3)

**Build:** `gameplay` and `level-editor` targets both compile without errors.
