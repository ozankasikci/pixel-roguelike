---
phase: 21-feature-module-reorganization
reviewed: 2026-04-07T12:00:00Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - apps/level_editor/main.cpp
  - apps/runtime/main.cpp
  - src/editor/scene/EditorPreviewWorld.cpp
  - src/editor/ui/inspectors/SceneSelectionInspector.cpp
  - src/game/behavior/ActionTypes.h
  - src/game/behavior/BehaviorSystem.cpp
  - src/game/behavior/BehaviorSystem.h
  - src/game/level/LevelBuilder.cpp
  - src/game/level/LevelBuilder.h
  - src/game/level/LevelDef.cpp
  - src/game/level/LevelDef.h
  - src/game/level/LevelLoader.cpp
  - src/game/modules/door/DoorActionHandler.cpp
  - src/game/modules/door/DoorActionHandler.h
  - src/game/modules/door/DoorActionTypes.h
  - src/game/modules/door/DoorAnimationSystem.cpp
  - src/game/modules/door/DoorAnimationSystem.h
  - src/game/modules/door/DoorComponents.h
  - src/game/modules/door/DoorMath.cpp
  - src/game/modules/door/DoorMath.h
  - src/game/modules/door/DoorModule.cpp
  - src/game/modules/door/DoorModule.h
  - src/game/modules/door/DoorSerializer.cpp
  - src/game/modules/door/DoorSerializer.h
  - src/game/modules/door/DoorSpawner.cpp
  - src/game/modules/door/DoorSpawner.h
  - src/game/modules/door/editor/DoorGroupInspector.cpp
  - src/game/modules/door/editor/DoorGroupInspector.h
  - src/game/prefabs/GameplayPrefabs.cpp
  - src/game/prefabs/GameplayPrefabs.h
  - src/game/runtime/RuntimeGameplay.cpp
  - src/game/runtime/RuntimeGameSession.cpp
  - tests/game/test_door_group_position.cpp
findings:
  critical: 0
  warning: 5
  info: 4
  total: 9
status: issues_found
---

# Phase 21: Code Review Report

**Reviewed:** 2026-04-07T12:00:00Z
**Depth:** standard
**Files Reviewed:** 33
**Status:** issues_found

## Summary

This review covers the Phase 21 feature module reorganization, which extracts door-related code from scattered locations into a self-contained `src/game/modules/door/` module. The refactoring introduces a clean module registration pattern (DoorModule.cpp), separates concerns into dedicated files (DoorActionHandler, DoorAnimationSystem, DoorComponents, DoorMath, DoorSerializer, DoorSpawner), and includes an editor inspector (DoorGroupInspector). The module also uses a plugin-style registration API for LevelDef keyword parsers and BehaviorSystem action handlers.

The code is generally well-structured and follows project conventions. No critical security or crash-risk issues were found. The warnings below primarily concern missing error handling for `std::stof` calls in the serializer, a potential logic inconsistency in the DoorActionHandler, duplicated helper code between LevelDef.cpp and DoorSerializer.cpp, and an edge case in the editor preview door group handling.

## Warnings

### WR-01: Uncaught std::stof exceptions in DoorSerializer optional field parsing

**File:** `src/game/modules/door/DoorSerializer.cpp:88-97`
**Issue:** The parser correctly wraps the initial position/yaw `std::stof` calls (line 80) in a try/catch, but the optional keyword fields (`open_angle`, `open_duration`, `interact_distance`, `interact_dot` at lines 88-97) call `std::stof` without exception handling. If a scene file contains a malformed value like `open_angle abc`, the process will crash with an uncaught exception rather than producing a parse error with file/line context.
**Fix:**
```cpp
} else if (tokens[index] == "open_angle" && index + 1 < tokens.size()) {
    float v = 0.0f;
    if (!tryParseFloatToken(tokens[index + 1], v)) {
        throwParseError(path, lineNumber, "invalid open_angle value");
    }
    dg.openAngle = v;
    index += 2;
```
Apply the same `tryParseFloatToken` + `throwParseError` pattern to `open_duration`, `interact_distance`, and `interact_dot` fields.

### WR-02: ToggleDoor closes a door that is currently moving toward open

**File:** `src/game/modules/door/DoorActionHandler.cpp:57`
**Issue:** The ToggleDoor handler at line 57 checks `isDoorFullyOpen(*state) || isDoorMoving(*state)` and closes the door in that case. This means if a door is partially opening (progress=0.3, targetState=Open), toggling it immediately reverses direction. While this may be intentional (responsive toggle), the `state->progress` is reset to `0.0f` on line 60 rather than preserving the current progress. This causes the door to snap from its current position to fully-open (or fully-closed) before starting the reverse animation, producing a visual jump. The same issue exists on the open branch (line 66, `state->progress = 0.0f`).
**Fix:** Remove the `state->progress = 0.0f` assignment when changing direction, or set it to `1.0f - state->progress` to reverse from the current position:
```cpp
// Close it - reverse from current position
state->targetState = DoorTargetState::Closed;
// Don't reset progress -- let it reverse from current position
```

### WR-03: Editor DoorGroup preview skips DoorConfig for locked doors, preventing unlock testing

**File:** `src/editor/scene/EditorPreviewWorld.cpp:241-251`
**Issue:** When rebuilding the editor preview world, the DoorGroup case (line 229) skips emitting `DoorConfigComponent` and `DoorStateComponent` for locked doors (`if (!dg.locked)`). This means locked doors in editor play preview have no door components at all and cannot be unlocked or animated via behavior scripts. The runtime path (DoorSpawner.cpp:87-97) does create an InteractableComponent for locked doors, but the editor preview path creates nothing -- the entity is a bare transform.
**Fix:** Consider always emitting DoorConfigComponent with the locked flag set, so that editor play-preview behavior scripts that unlock doors (e.g., via an UnlockDoor action) can find the component:
```cpp
DoorConfigComponent config;
config.interactDistance = dg.interactDistance;
config.interactDotThreshold = dg.interactDotThreshold;
config.openDuration = dg.openDuration;
config.openAngle = dg.openAngle;
config.locked = dg.locked;
config.lockedPrompt = dg.lockedPrompt;
registry_.emplace<DoorConfigComponent>(doorRoot, config);
registry_.emplace<DoorStateComponent>(doorRoot);
```

### WR-04: DoorActionHandler modifies shared DoorConfigComponent::openDuration from action params

**File:** `src/game/modules/door/DoorActionHandler.cpp:22-24`
**Issue:** The OpenDoor handler writes `config->openDuration = doorParams->duration` at line 23. This permanently mutates the DoorConfigComponent for the entity. If a behavior script triggers OpenDoor with `duration 0.5` and later a different script triggers it with no duration param, the door retains the previously-written 0.5s duration rather than its original configured value. This is a latent bug if multiple scripts target the same door with different durations, and it also means baseline state restoration (RuntimeGameSession::restoreBaselineState) does not restore the config, only the state. The same pattern appears in the ToggleDoor handler at line 69-70.
**Fix:** Store the overridden duration in DoorStateComponent as a per-animation override rather than mutating the config:
```cpp
// In DoorStateComponent, add:
//   float durationOverride = 0.0f;  // 0 = use config default
// In DoorActionHandler:
if (doorParams && doorParams->duration > 0.0f) {
    state->durationOverride = doorParams->duration;
}
// In DoorAnimationSystem, use the override if set:
const float duration = (state.durationOverride > 0.0f) ? state.durationOverride : config.openDuration;
```

### WR-05: RuntimeGameplay::updateRuntimeBehaviors does not handle delayed actions

**File:** `src/game/runtime/RuntimeGameplay.cpp:430-457`
**Issue:** The `updateRuntimeBehaviors` function (used by RuntimeGameSession) processes `onActivate` actions directly without scheduling delayed actions. If an action has a non-zero `entry.delay`, it is still executed immediately (line 449-453 calls the handler without checking `action.delay`). In contrast, `BehaviorSystem::executeActionList` (used by the Application path) correctly handles delays via `schedulePendingAction`. This means doors or other actions with delays will fire immediately in editor play-preview but correctly delayed in the full runtime.
**Fix:** Either reuse the delay scheduling logic or check `action.delay > 0.0f` and skip immediate execution:
```cpp
if (action.delay > 0.0f) {
    // Skip -- delayed actions need a scheduler, which
    // RuntimeGameSession does not currently have.
    continue;
}
```
At minimum, document this limitation. A fuller fix would add a pending-action queue to RuntimeGameSession.

## Info

### IN-01: Duplicated helper functions between LevelDef.cpp and DoorSerializer.cpp

**File:** `src/game/modules/door/DoorSerializer.cpp:13-66`
**Issue:** `tryParseFloatToken`, `parseNodeMetadata`, `formatFloat`, and `appendNodeMetadata` are duplicated verbatim from `src/game/level/LevelDef.cpp`. This creates a maintenance risk -- fixes to one copy may not be applied to the other.
**Fix:** Extract these helpers into a shared utility header (e.g., `game/content/ParseUtils.h` already exists and provides `throwParseError`; the float/vec3 parsing and formatting helpers could live there too).

### IN-02: Unused `PlacementBase` struct in LevelDef.h

**File:** `src/game/level/LevelDef.h:18-23`
**Issue:** `PlacementBase` is declared with a comment saying it is "Used as the parameter type for parser helpers" but it is never actually used in any of the reviewed files. Each placement struct defines its own `nodeId` and `parentNodeId` fields instead of inheriting from or using PlacementBase.
**Fix:** Either remove PlacementBase or refactor the placement structs to use it as intended.

### IN-03: DoorGroupInspector nodeId-based leaf detection is fragile

**File:** `src/game/modules/door/editor/DoorGroupInspector.cpp:125-148`
**Issue:** The inspector finds leaf meshes by iterating all document objects and comparing `meshPlacement.parentNodeId` against `dg.nodeId`. If the door group has no `nodeId` assigned (empty string), the loop at line 125 is correctly skipped, but the user sees a warning about no leaf meshes. This is technically correct behavior, but the warning text does not hint that assigning a node ID is the fix.
**Fix:** Update the warning text to be more actionable:
```cpp
if (dg.nodeId.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f),
                       "Assign a node ID to link leaf meshes.");
} else if (!foundLeaf) {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f),
                       "No leaf meshes found -- this door won't animate.");
}
```

### IN-04: Static global registries lack thread-safety documentation

**File:** `src/game/behavior/BehaviorSystem.cpp:23-27` and `src/game/level/LevelDef.cpp:618-621`
**Issue:** Both `actionHandlerRegistry()` and `keywordRegistry()` use function-local static `std::unordered_map` instances. These are called during `registerDoorModule()` at startup and during level loading. While the current single-threaded usage is safe, these global registries have no documentation or assertions preventing concurrent access, which could become an issue if module registration or level loading is ever parallelized.
**Fix:** Add a comment noting single-threaded-only assumption, or protect with a mutex if future threading is planned.

---

_Reviewed: 2026-04-07T12:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
