---
phase: 04-make-engine-fully-generic
plan: 02
subsystem: engine-input
tags: [input, engine-boundary, action-map, refactor]
dependency_graph:
  requires: []
  provides: [ActionMap, InputSystem-no-game-dep]
  affects: [engine_input, gameplay, game-systems, editor-preview]
tech_stack:
  added: [ActionMap (engine/input)]
  patterns: [ActionMap named bindings, direct InputSystem polling]
key_files:
  created:
    - src/engine/input/ActionMap.h
    - src/engine/input/ActionMap.cpp
  modified:
    - src/engine/input/InputSystem.h
    - src/engine/input/InputSystem.cpp
    - src/engine/CMakeLists.txt
    - src/game/CMakeLists.txt
    - src/game/runtime/RuntimeGameSession.h
    - src/game/runtime/RuntimeGameSession.cpp
    - src/game/runtime/RuntimeGameplay.h
    - src/game/runtime/RuntimeGameplay.cpp
    - src/game/systems/CameraSystem.h
    - src/game/systems/CameraSystem.cpp
    - src/game/systems/PlayerMovementSystem.h
    - src/game/systems/PlayerMovementSystem.cpp
    - src/game/systems/RenderSystem.h
    - src/game/systems/InteractionSystem.h
    - src/game/systems/InteractionSystem.cpp
    - src/game/systems/InventorySystem.h
    - src/game/systems/InventorySystem.cpp
    - src/game/systems/DoorSystem.h
    - src/game/systems/DoorSystem.cpp
    - src/game/systems/CheckpointSystem.h
    - src/game/systems/CheckpointSystem.cpp
    - src/editor/core/EditorRuntimePreviewSession.h
    - src/editor/core/EditorRuntimePreviewSession.cpp
    - tests/game/test_runtime_game_session.cpp
decisions:
  - "InputSystem owns all raw input state arrays directly (no RuntimeInputState wrapper) — satisfies D-04"
  - "ActionMap update() receives const array refs each frame from InputSystem::update(), not GLFW handles — keeps ActionMap GLFW-free"
  - "RuntimeGameSession owns InputSystem by value and registers game actions in constructor — ensures actions are bound before first tick"
  - "RuntimeInputState remains in game layer (game/runtime/) with RuntimeInputState.cpp compiled into gameplay target — preserves backward compatibility for tests and editor"
  - "InputSystem exposes setter methods (setKeyPressed, beginFrame, reset) for editor manual injection — editor preview does not use GLFW callbacks"
metrics:
  duration: "~3 hours (multi-session)"
  completed: "2026-03-29"
  tasks_completed: 3
  tasks_total: 3
  files_changed: 25
---

# Phase 04 Plan 02: Decouple engine_input from RuntimeInputState — ActionMap + D-04 Summary

ActionMap added to engine layer with named action bindings; InputSystem internalizes all raw key/mouse state directly, removing the RuntimeInputState game-layer dependency from engine_input per D-04.

## What Was Built

### ActionMap (D-03)

New `src/engine/input/ActionMap.h/cpp` — pure engine class with no game dependencies:

- `bind(action, ActionBinding{.keys={...}, .mouseButtons={...}})` — register named action
- `isPressed(action)` / `isJustPressed(action)` / `isJustReleased(action)` — query action state
- `update(currentKeys, previousKeys, currentButtons, previousButtons)` — called by InputSystem each frame

ActionBinding uses designated initializers (C++20). ActionMap is GLFW-free — it only operates on bool arrays.

### InputSystem internalized state (D-04)

`InputSystem` now owns raw state directly:
- `std::array<bool, 512> currentKeys_`, `previousKeys_` — key state
- `std::array<bool, 8> currentButtons_`, `previousButtons_` — mouse button state
- `ActionMap actionMap_` — owned action map
- Public setter API for editor injection: `beginFrame()`, `reset()`, `setKeyPressed()`, `setMouseButtonPressed()`, `setMousePosition()`, `setMouseDelta()`, `setScrollDelta()`, `setCursorLocked()`, `setWantsCaptureMouse()`
- `static constexpr int kMaxKeys = 512; kMaxButtons = 8;`

`InputSystem::update()` calls `actionMap_.update(currentKeys_, previousKeys_, currentButtons_, previousButtons_)` at end of each frame.

### engine_input CMake target (D-04)

```cmake
add_library(engine_input STATIC
    input/InputSystem.cpp
    input/ActionMap.cpp
)
```

Zero game-layer source files. `RuntimeInputState.cpp` moved to `gameplay` target.

### Game action registration (D-03)

`RuntimeGameSession` constructor registers 10 named actions:
`move_forward`, `move_backward`, `strafe_left`, `strafe_right`, `sprint`, `jump`, `interact`, `inventory`, `attack`, `screenshot`

### Game systems update

All 7 game systems (Camera, PlayerMovement, Render, Interaction, Inventory, Door, Checkpoint), the editor preview session, and the test `test_runtime_game_session` updated to use `InputSystem` directly instead of `RuntimeInputState`.

## Verification

All 7 plan checks pass:

1. `grep -r "game/" src/engine/input/` — no results (PASS)
2. `grep "RuntimeInputState" src/engine/CMakeLists.txt` — no results (PASS)
3. `grep "RuntimeInputState" src/engine/input/InputSystem.h` — no results (PASS)
4. `ActionMap.h` exists (PASS)
5. `grep "actionMap" src/game/runtime/RuntimeGameSession.cpp` — returns `auto& actions = input.actionMap();` (PASS)
6. `cmake --build build` — full project builds, all targets including level-editor and pixel-roguelike (PASS)
7. `grep -rn "\.state()" src/game/systems/` — no results (PASS)

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1 | `1311831` | Create ActionMap, internalize raw input state in InputSystem, remove RuntimeInputState dependency |
| Task 2 | `5685243` | Update core runtime files to use InputSystem directly, register named actions |
| Task 3 | `e83f96d` | Update all game systems, editor, and tests to use InputSystem directly |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed test_runtime_game_session.cpp using RuntimeInputState**
- **Found during:** Task 3 — build verification revealed test file still included and instantiated `RuntimeInputState`
- **Issue:** `tests/game/test_runtime_game_session.cpp` included `game/runtime/RuntimeInputState.h` and declared `RuntimeInputState input;`, causing 5 compile errors since free functions now accept `InputSystem&`
- **Fix:** Replaced include with `engine/input/InputSystem.h` and changed `RuntimeInputState input;` to `InputSystem input;`
- **Files modified:** `tests/game/test_runtime_game_session.cpp`
- **Commit:** `e83f96d`

**2. [Rule 1 - Bug] Transient build error (assimp parallel rename)**
- **Found during:** Task 3 — first full build attempt failed with filesystem rename error on assimp zip.c.o
- **Fix:** Retry build — the race condition resolved on second invocation
- **No files modified**

### Pre-existing Failures (Out of Scope)

`test_content_registry` was already failing before this plan with `Assertion failed: registry.findEnvironment("neutral") != nullptr`. Logged to deferred items — unrelated to input layer changes.

## Known Stubs

None — all changes are structural refactors and the ActionMap is fully wired. The `RuntimeInputState` type still exists as a game-layer type and is compiled into gameplay, but it is no longer used by engine_input or any of the updated game systems. Future plans may remove it entirely once no code depends on it.

## Self-Check: PASSED

Files verified:
- `src/engine/input/ActionMap.h` — FOUND
- `src/engine/input/ActionMap.cpp` — FOUND
- `src/engine/input/InputSystem.h` — contains `ActionMap& actionMap()`, no `RuntimeInputState`

Commits verified:
- `1311831` — FOUND
- `5685243` — FOUND
- `e83f96d` — FOUND
