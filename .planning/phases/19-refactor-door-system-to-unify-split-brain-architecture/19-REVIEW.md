---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
reviewed: 2026-04-07T12:00:00Z
depth: standard
files_reviewed: 31
files_reviewed_list:
  - src/editor/core/EditorCommand.cpp
  - src/editor/render/EditorScenePreviewRenderer.cpp
  - src/editor/scene/EditorPreviewWorld.cpp
  - src/editor/scene/EditorSceneDocument.cpp
  - src/editor/scene/EditorSceneDocument.h
  - src/editor/ui/inspectors/DoorGroupInspector.cpp
  - src/editor/ui/inspectors/DoorGroupInspector.h
  - src/editor/ui/inspectors/MeshInspector.cpp
  - src/editor/ui/inspectors/PrefabInspector.cpp
  - src/editor/ui/inspectors/SceneSelectionInspector.cpp
  - src/editor/viewport/EditorViewportInteraction.cpp
  - src/game/behavior/BehaviorSystem.cpp
  - src/game/behavior/DoorAnimationSystem.cpp
  - src/game/behavior/DoorAnimationSystem.h
  - src/game/components/DoorConfigComponent.h
  - src/game/components/DoorStateComponent.h
  - src/game/content/ContentRegistry.cpp
  - src/game/content/ContentRegistry.h
  - src/game/level/LevelBuilder.cpp
  - src/game/level/LevelBuilder.h
  - src/game/level/LevelDef.cpp
  - src/game/level/LevelDef.h
  - src/game/prefabs/GameplayPrefabData.h
  - src/game/prefabs/GameplayPrefabs.cpp
  - src/game/prefabs/GameplayPrefabs.h
  - src/game/runtime/RuntimeGameplay.cpp
  - src/game/runtime/RuntimeGameplay.h
  - src/game/runtime/RuntimeGameSession.cpp
  - tests/game/CMakeLists.txt
  - tests/game/test_cathedral_prefabs.cpp
  - tests/game/test_content_registry.cpp
  - tests/game/test_door_group_position.cpp
findings:
  critical: 0
  warning: 5
  info: 5
  total: 10
status: issues_found
---

# Phase 19: Code Review Report

**Reviewed:** 2026-04-07T12:00:00Z
**Depth:** standard
**Files Reviewed:** 31
**Status:** issues_found

## Summary

This review covers the unified door system refactor (phase 19) including the new `LevelDoorPlacement` data type, `DoorConfigComponent`/`DoorStateComponent`/`DoorLeafComponent` ECS components, `DoorAnimationSystem`, `LevelBuilder::addDoorGroup`, pivot math in `makePivotLeafModel`, editor DoorGroup inspector, and associated editor preview/viewport integration.

The architecture is sound: the door system is self-contained with clear data flow from `.scene` file through `LevelDef` to ECS entities. The pivot math is well-tested. The editor integration covers selection bubbling, gizmo manipulation, and inspector UI.

Five warnings were found, primarily around missing null-safety for ECS entity references stored in components, an incomplete door animation reset during baseline restore, and a potential division-by-zero in the FlickerLight behavior action. Five informational items flag dead code, code duplication, and minor quality improvements.

## Warnings

### WR-01: DoorConfigComponent stores raw entity handles without validation guard

**File:** `src/game/behavior/DoorAnimationSystem.cpp:26-28`
**Issue:** `updateDoorLeaf` is called with `config.leftLeaf` and `config.rightLeaf` from `DoorConfigComponent`, which default to `entt::null`. However, the function only checks whether the entity has certain components (`try_get<MeshComponent>`, etc.) -- it does not check `registry.valid(entity)` before accessing the entity. If a leaf entity is destroyed (e.g., during hot-reload or editor teardown) while the door root still exists, `registry.try_get` on an invalid entity handle is undefined behavior in EnTT.
**Fix:**
```cpp
void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    if (entity == entt::null || !registry.valid(entity)) {
        return;
    }
    auto* mesh = registry.try_get<MeshComponent>(entity);
    // ... rest unchanged
}
```

### WR-02: DoorAnimationSystem::init calls updateDoorLeaf on potentially null leaf entities

**File:** `src/game/behavior/DoorAnimationSystem.cpp:112-119`
**Issue:** During `DoorAnimationSystem::init`, the system iterates all doors and calls `updateDoorLeaf(registry, config.leftLeaf, 0.0f)` and `updateDoorLeaf(registry, config.rightLeaf, 0.0f)`. The `rightLeaf` is always `entt::null` for single-leaf doors (set in `LevelBuilder::addDoorGroup` line 359). While `updateDoorLeaf` currently returns early via `try_get` returning nullptr, this relies on EnTT allowing `try_get` on `entt::null` without crashing -- which is not guaranteed across all EnTT versions. This is the same root cause as WR-01.
**Fix:** Same fix as WR-01 -- add the `entity == entt::null` early-return guard.

### WR-03: Baseline state restore does not reset door leaf transforms

**File:** `src/game/runtime/RuntimeGameSession.cpp:399-405`
**Issue:** `restoreBaselineState` restores `DoorStateComponent` (progress, targetState) for each door root entity, but does not call `updateDoorLeaf` or `tickDoorAnimation` to re-apply the leaf mesh/collider transforms corresponding to the restored progress. After `restoreBaselineState`, the leaf mesh `modelOverride` and collider position remain at whatever state they were in before the reset. This means a door that was mid-swing when the player resets will show the correct `progress=0` state component but the leaf meshes remain visually at the old angle until the next `tickDoorAnimation` call -- which happens to occur in the next `tick()`. In practice the visual glitch is a single frame, but it violates the invariant that restore produces a consistent state.
**Fix:** After the door state restore loop, re-initialize leaf transforms:
```cpp
// After restoring door states, re-sync leaf transforms
auto doorView2 = registry_.view<DoorConfigComponent, DoorStateComponent>();
for (auto [entity, config, state] : doorView2.each()) {
    updateDoorLeaf(registry_, config.leftLeaf, state.progress);
    updateDoorLeaf(registry_, config.rightLeaf, state.progress);
}
```

### WR-04: FlickerLight action can produce extreme intensity values

**File:** `src/game/behavior/BehaviorSystem.cpp:275-278`
**Issue:** The FlickerLight action implementation toggles intensity: `light->intensity = (light->intensity > 0.5f) ? 0.1f : light->intensity * 10.0f;`. If the light's intensity is very small but non-zero (e.g., 0.001), the multiply path produces `0.01`, and repeated flickering will converge to cycling between `0.1` and `1.0` -- which is acceptable. However, if `light->intensity` is exactly 0.0 (e.g., a disabled light), this produces 0.0 * 10 = 0.0, and the light gets stuck at 0.0 permanently with no recovery path. The `FlickerLightParams` struct has `duration` and `rate` fields that are parsed but never used by this implementation.
**Fix:** Add a minimum intensity floor and use the parsed params:
```cpp
case ActionType::FlickerLight: {
    if (target == entt::null) break;
    auto* light = registry.try_get<LightComponent>(target);
    if (!light) break;
    const float minIntensity = 0.1f;
    light->intensity = (light->intensity > 0.5f)
        ? minIntensity
        : std::max(light->intensity * 10.0f, minIntensity);
    break;
}
```

### WR-05: MeshInspector prompt buffer truncation without user feedback

**File:** `src/editor/ui/inspectors/MeshInspector.cpp:108-109`
**Issue:** The interaction prompt text is copied into a 64-byte `char` buffer via `std::snprintf`. If the prompt text exceeds 63 characters, it is silently truncated. The DoorGroupInspector uses 128 bytes for its name buffer and 256 bytes for the locked prompt buffer, suggesting awareness of the issue for longer text. 64 bytes for a user-facing prompt string ("Press E to interact") is borderline tight -- a designer adding a descriptive prompt like "Press E to examine the ancient inscription on the wall" (53 chars) is close to the limit.
**Fix:** Increase the buffer to 256 bytes to match the pattern in DoorGroupInspector:
```cpp
char promptBuf[256];
std::snprintf(promptBuf, sizeof(promptBuf), "%s", mesh.interactable->promptText.c_str());
```

## Info

### IN-01: Duplicated decomposeTransformMatrix function across three files

**File:** `src/editor/scene/EditorSceneDocument.cpp:25-39`, `src/editor/scene/EditorPreviewWorld.cpp:76-90`, `src/editor/viewport/EditorViewportInteraction.cpp:75-89`
**Issue:** The same `decomposeTransformMatrix` helper function (decompose + extract Euler angles) is defined as an anonymous-namespace function in three separate files. This violates DRY and creates a maintenance risk if the decomposition logic needs to change.
**Fix:** Move to a shared utility (e.g., `engine/core/MathUtils.h`) and call from all three sites.

### IN-02: Unused FlickerLightParams fields

**File:** `src/game/behavior/BehaviorSystem.cpp:273-278`
**Issue:** The `FlickerLightParams` struct has `duration` and `rate` fields that are parsed from scene files and serialized back, but the `FlickerLight` action handler in `BehaviorSystem::executeAction` ignores them entirely. The comment "Future: could use a dedicated FlickerState component" acknowledges this is incomplete.
**Fix:** Either implement proper time-based flickering using the parsed params, or add a TODO comment at the parse site warning that these fields are currently unused at runtime.

### IN-03: updateRuntimeBehaviors is a dead stub

**File:** `src/game/runtime/RuntimeGameplay.cpp:428-432`
**Issue:** `updateRuntimeBehaviors` is declared in `RuntimeGameplay.h` and defined as an empty function with a comment explaining it is a retained stub. It is not called from any reviewed file (`RuntimeGameSession::tick` does not call it). This is dead code.
**Fix:** Remove the declaration from the header and the stub definition, or add a deprecation comment if callers exist outside the reviewed scope.

### IN-04: EditorSelectionCommand stores raw pointer to external vector

**File:** `src/editor/core/EditorCommand.cpp:63-64`
**Issue:** `EditorSelectionCommand` stores a `std::vector<std::uint64_t>*` raw pointer (`selectedIds_`) passed by the caller. If the pointed-to vector is destroyed or moved before undo/redo is called, the pointer becomes dangling. This is currently safe because the selection vector outlives the command stack in the editor lifecycle, but the raw pointer makes this contract implicit and fragile.
**Fix:** Document the lifetime requirement with a comment, or consider passing a reference to the owning object instead.

### IN-05: Test file test_door_group_position.cpp uses hardcoded mesh geometry constants

**File:** `tests/game/test_door_group_position.cpp:37-41`
**Issue:** The test uses `leafScale(0.22f, ...)` and `meshCenter(-1.97f, 0.0f, -0.1f)` which are approximations of real mesh geometry. These values do not match the actual `kLeafScale(1.0f)` and runtime-computed `meshCenter` used in `LevelBuilder::addDoorGroup`. The test still passes because it tests the math properties (closed center at group position, hinge invariant) rather than absolute values, but the mismatch between test constants and production constants could mask regressions.
**Fix:** Either align test constants with the actual values used in `LevelBuilder::addDoorGroup`, or add a comment explaining why different constants are intentionally used (to test math generality).

---

_Reviewed: 2026-04-07T12:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
