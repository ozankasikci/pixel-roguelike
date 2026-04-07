# Phase 21: Feature Module Reorganization — Research

**Researched:** 2026-04-07
**Domain:** C++ codebase reorganization — folder-per-feature module pattern
**Confidence:** HIGH (verified by direct source inspection)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01:** Co-locate ALL code for a feature in one directory, including editor-only code (DoorGroupInspector). Organizational clarity over strict layer boundary. `src/game/modules/door/` contains game logic, animation system, spawner, serializer, AND DoorGroupInspector.

**D-02:** Editor-only files compiled conditionally via CMake source list only — no `#ifdef` preprocessor macros in source code.

**D-03:** Each module is its own CMake static library. `game_module_door` declares its dependencies explicitly (engine_core, engine_rendering, engine_physics, gameplay).

**D-04:** `editor` target links `game_module_door` with editor source files included. `pixel-roguelike` target links `game_module_door` without editor source files. `gameplay` target does NOT include module code.

**D-05:** Modules register keyword parser callbacks at startup. LevelDef's parser delegates to registered handlers. LevelDef.cpp no longer `#include`s door headers.

**D-06:** Registration happens during `registerDoorModule()` called from app startup. Parser maintains `std::unordered_map<std::string, ParseCallback>` for keyword→handler lookup.

**D-07:** Serialization also uses registered callbacks — each module provides both parser and serializer for its keywords. Full round-trip ownership within the module.

**D-08:** Door-specific action types (OpenDoor, CloseDoor, ToggleDoor) and DoorActionParams move out of `ActionTypes.h` into the door module. Central `ActionTypes.h` retains only shared/base types.

**D-09:** BehaviorSystem dispatches to registered action handlers. Door module registers its action handler at startup.

**D-10:** Files that move into `src/game/modules/door/`:
  - DoorConfigComponent.h, DoorStateComponent.h, DoorLeafComponent.h (from `components/`)
  - DoorAnimationSystem.h/.cpp (from `behavior/`)
  - Door spawning logic from LevelBuilder.cpp → DoorSpawner.cpp
  - Door serialization logic from LevelDef.cpp → DoorSerializer.cpp
  - DoorGroupInspector.h/.cpp (from `editor/ui/inspectors/`)
  - makePivotLeafModel() from GameplayPrefabs → DoorMath or similar
  - DoorActionParams and door action types from ActionTypes.h

**D-11:** Door-related test files move to `tests/game/modules/door/`.

### Claude's Discretion
- Internal file naming within the module directory
- Exact registration API design (function signatures, init ordering)
- How to extract door spawning logic from LevelBuilder.cpp cleanly
- Whether the parser registration map lives in LevelDef or a separate ParserRegistry class
- How to handle the `door_group` keyword migration (keep vs rename)
- Whether a `modules/` README or template is needed

### Deferred Ideas (OUT OF SCOPE)
- Migrating checkpoints, cameras, inventory, and other interactive objects to module pattern
- `ModuleTemplate/` directory or documentation for creating new modules
- Module generator script/tool
</user_constraints>

---

## Summary

Phase 21 is a pure file reorganization and coupling-elimination phase. No behavior changes, no new features. The goal is to collapse 13+ door-related files spread across 7 directories into a single `src/game/modules/door/` directory that a single CMake target (`game_module_door`) exposes.

The current door system is well-defined after Phase 19 unified the implementation. There is exactly one animation system (`DoorAnimationSystem`), one spawner (`LevelBuilder::addDoorGroup`), and one state machine split across `BehaviorSystem::executeAction` (for app-based path) and `RuntimeGameplay::updateRuntimeBehaviors` (for session-based path). This **split brain** in action dispatch is the primary structural problem to eliminate alongside the file scatter.

The critical complication is the **registration protocol** (D-05 through D-09): LevelDef.cpp currently owns the door parser inline; BehaviorSystem.cpp directly handles door action types in a switch. Both must be made extensible by callback registration so LevelDef and BehaviorSystem no longer include door headers. This is the non-trivial architectural work. The file moves themselves are mechanical.

**Primary recommendation:** Plan in three sequential waves — (1) create the module directory and file structure with a working CMake target, (2) implement the registration protocol in LevelDef and BehaviorSystem, (3) wire up the module registration at application startup and remove all old door references from their original locations.

---

## Architecture Patterns

### Recommended Module Structure

```
src/game/modules/
└── door/
    ├── DoorComponents.h          # DoorConfigComponent, DoorStateComponent, DoorLeafComponent
    ├── DoorActionTypes.h         # DoorActionParams, OpenDoor/CloseDoor/ToggleDoor entries
    ├── DoorAnimationSystem.h     # tickDoorAnimation() + DoorAnimationSystem class
    ├── DoorAnimationSystem.cpp
    ├── DoorMath.h                # makePivotLeafModel() declaration
    ├── DoorMath.cpp              # makePivotLeafModel() implementation
    ├── DoorSpawner.h             # spawnDoorGroup() — extracted from LevelBuilder::addDoorGroup
    ├── DoorSpawner.cpp
    ├── DoorSerializer.h          # parseDoorGroup(), serializeDoorGroup() callbacks
    ├── DoorSerializer.cpp
    ├── DoorActionHandler.h       # registerDoorActionHandler() callback
    ├── DoorActionHandler.cpp     # OpenDoor/CloseDoor/ToggleDoor logic
    ├── DoorModule.h              # registerDoorModule() — single entry point
    ├── DoorModule.cpp            # calls all registrations
    └── editor/
        ├── DoorGroupInspector.h  # (unchanged API, moved)
        └── DoorGroupInspector.cpp
```

Note on `DoorComponents.h`: all three component structs are small and used together by all door code. Merging into one header reduces include count without losing clarity. Alternatively keep them as separate files — both are valid per Claude's Discretion.

### Registration Protocol Design

The two extension points that need callback registration:

**Parser registration (LevelDef.cpp):**

```cpp
// [VERIFIED: direct source inspection of src/game/level/LevelDef.cpp]
// Current: LevelDef.cpp hardcodes "door_group" parsing inline at line 971.
// After: LevelDef maintains a map and delegates.

// In LevelDef.h (additions):
using LevelDefParseCallback = std::function<void(LevelDef&, const std::string& path,
                                                  int lineNumber,
                                                  const std::vector<std::string>& tokens)>;
using LevelDefSerializeCallback = std::function<void(std::ostringstream&, const LevelDef&)>;

void registerLevelDefParser(const std::string& keyword, LevelDefParseCallback);
void registerLevelDefSerializer(LevelDefSerializeCallback);
```

Or alternatively, the registration map lives in a separate `ParserRegistry` class (per Claude's Discretion). The key constraint: `LevelDef.cpp` must not `#include` door headers after this change.

**Action handler registration (BehaviorSystem.cpp + RuntimeGameplay.cpp):**

```cpp
// [VERIFIED: direct source inspection]
// BehaviorSystem::executeAction() has a switch at line 170 that directly handles
// ActionType::OpenDoor, ::CloseDoor, ::ToggleDoor using DoorConfigComponent/DoorStateComponent.
// RuntimeGameplay::updateRuntimeBehaviors() has a DUPLICATE switch at line 450.
// Both must delegate to registered handlers.

using ActionHandler = std::function<void(Application&, entt::entity source, ActionEntry&)>;
void registerBehaviorActionHandler(ActionType type, ActionHandler handler);

// For session-based path (RuntimeGameplay):
using SessionActionHandler = std::function<void(entt::registry&, entt::entity, ActionEntry&)>;
void registerSessionActionHandler(ActionType type, SessionActionHandler handler);
```

### Module Registration Entry Point

```cpp
// DoorModule.h
void registerDoorModule(LevelDefRegistry& levelDefRegistry,
                        BehaviorRegistry& behaviorRegistry);

// DoorModule.cpp calls:
// - registerLevelDefParser("door_group", parseDoorGroup)
// - registerLevelDefSerializer(serializeDoorGroups)
// - registerBehaviorActionHandler(ActionType::OpenDoor, handleOpenDoor)
// - registerBehaviorActionHandler(ActionType::CloseDoor, handleCloseDoor)
// - registerBehaviorActionHandler(ActionType::ToggleDoor, handleToggleDoor)
```

Registration is called from `Application::init()` in both `apps/runtime/main.cpp` and `apps/level_editor/main.cpp`.

### CMake Target Structure

```cmake
# src/game/modules/door/CMakeLists.txt

# Core sources (compiled in all targets)
set(DOOR_MODULE_CORE_SOURCES
    DoorAnimationSystem.cpp
    DoorMath.cpp
    DoorSpawner.cpp
    DoorSerializer.cpp
    DoorActionHandler.cpp
    DoorModule.cpp
)

# Editor-only sources (only when building editor target)
set(DOOR_MODULE_EDITOR_SOURCES
    editor/DoorGroupInspector.cpp
)

add_library(game_module_door STATIC ${DOOR_MODULE_CORE_SOURCES})
target_include_directories(game_module_door PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_door PUBLIC
    engine_core
    engine_rendering
    engine_physics
    gameplay           # for LevelBuilder, BehaviorSystem, ActionTypes base
)
```

Editor-only sources are added to the `editor` target's source list in `src/editor/CMakeLists.txt`, not to `game_module_door`:

```cmake
# src/editor/CMakeLists.txt addition:
add_library(editor STATIC
    ...
    ${CMAKE_SOURCE_DIR}/src/game/modules/door/editor/DoorGroupInspector.cpp
    ...
)
```

This satisfies D-02 (no `#ifdef`) and D-04 (editor links the file directly without it polluting the game target).

### `gameplay` Target After Refactor

`gameplay` CMakeLists.txt removes:
- `behavior/DoorAnimationSystem.cpp`

`LevelBuilder.cpp` removes `addDoorGroup()` entirely — it delegated to `DoorSpawner.cpp`. `LevelBuilder.h` drops the `addDoorGroup` declaration. LevelDef.cpp loses the inline `door_group` parser block and serializer block. ActionTypes.h loses `DoorActionParams` and the three door `ActionType` enum values.

### Anti-Patterns to Avoid

- **Circular dependency:** `game_module_door` links `gameplay`, but `gameplay` must NOT link `game_module_door`. The dependency is one-way. Any attempt to put `#include "game/modules/door/DoorComponents.h"` into LevelBuilder.h would create a cycle.

- **`#ifdef EDITOR` in source:** D-02 explicitly forbids this. Use CMake source list separation only.

- **Registering from a constructor/global init:** Registration must be called explicitly from app startup (`registerDoorModule()`), not from C++ static initializers or global constructors, which have undefined init order.

- **Forgetting the session-based action path:** `RuntimeGameSession::tick()` calls `tickDoorAnimation` (line 213) and `updateRuntimeBehaviors` (line 212). The `updateRuntimeBehaviors` function in `RuntimeGameplay.cpp` has its OWN duplicate switch over door action types. This duplicate MUST be removed and replaced with the same registration dispatch. Leaving it is leaving the split-brain intact.

---

## Current State Inventory

### Door-Related Files (verified by source inspection)

| File | Current Location | What It Contains | Destination |
|------|-----------------|------------------|-------------|
| DoorConfigComponent.h | `src/game/components/` | Spawn-time config (openAngle, openDuration, etc.) | `modules/door/DoorComponents.h` or separate |
| DoorStateComponent.h | `src/game/components/` | Runtime state (progress, targetState); free helpers | Same |
| DoorLeafComponent.h | `src/game/components/` | Per-leaf animation data (pivot, closedYaw, openYaw) | Same |
| DoorAnimationSystem.h/.cpp | `src/game/behavior/` | Animation tick; `tickDoorAnimation()` free function | `modules/door/` |
| ActionTypes.h | `src/game/behavior/` | **Shared** file; contains door types AND generic types | Door parts move out; shared types stay |
| BehaviorSystem.cpp | `src/game/behavior/` | Door action cases in `executeAction()` switch | Door cases extracted to `DoorActionHandler.cpp` |
| LevelBuilder.cpp | `src/game/level/` | `addDoorGroup()` method — 116 lines of door spawning logic | Extracted to `DoorSpawner.cpp` |
| LevelDef.cpp | `src/game/level/` | `door_group` parser (lines 971-1018); serializer (lines 1422-1435) | Extracted to `DoorSerializer.cpp` |
| LevelDef.h | `src/game/level/` | `LevelDoorPlacement` struct; `doors` field in `LevelDef` | `LevelDoorPlacement` may need to stay in LevelDef.h for the LevelDef aggregate, or move to door module and forward-declare |
| GameplayPrefabs.h/.cpp | `src/game/prefabs/` | `makePivotLeafModel()` (44 lines) | `modules/door/DoorMath.cpp` |
| DoorGroupInspector.h/.cpp | `src/editor/ui/inspectors/` | ImGui inspector for `LevelDoorPlacement` | `modules/door/editor/` |
| RuntimeGameplay.cpp | `src/game/runtime/` | Duplicate door action switch at line 450 | Door cases replaced by registered handler calls |
| RuntimeGameSession.cpp | `src/game/runtime/` | Includes `DoorAnimationSystem.h`; calls `tickDoorAnimation()` | Include path changes to `game/modules/door/` |
| EditorPreviewWorld.cpp | `src/editor/scene/` | DoorGroup case at line 230 creates DoorConfigComponent directly | Uses DoorSpawner or unchanged — still needs door component headers |

### `LevelDoorPlacement` Struct — Special Case

`LevelDoorPlacement` is defined in `LevelDef.h` and is the data type that `LevelDef::doors` holds. It is also the payload type in `EditorSceneDocument` (via `EditorSceneObjectKind::DoorGroup`). Two options:

1. **Keep `LevelDoorPlacement` in `LevelDef.h`** — LevelDef.h remains the authoritative type for all placement data. Door module includes `LevelDef.h`. This is the path of least resistance and keeps `EditorSceneDocument` changes minimal.

2. **Move `LevelDoorPlacement` to door module header** — LevelDef.h forward-declares or includes from the module. This is purer but requires EditorSceneDocument.h to include from `game/modules/door/`, crossing the layer boundary.

Option 1 is strongly recommended to avoid ripple effects in `EditorSceneDocument`, `EditorSceneSerializer`, `EditorSelectionSystem`, and other editor files that reference `LevelDoorPlacement` indirectly through `LevelDef.h`.

### ActionTypes.h Split — What Stays vs. What Moves

**Stays in `ActionTypes.h`** (used by BehaviorSystem, other future modules):
- `ActionType` enum (but remove OpenDoor/CloseDoor/ToggleDoor entries — or keep them as placeholders if other code needs the enum values)
- `SoundActionParams`, `LightActionParams`, `FlickerLightParams`, `MessageActionParams`
- `DelayActionParams`, `EntityToggleParams`, `EventActionParams`, `PlayerLockParams`, `TeleportPlayerParams`
- `ActionParams` variant (must be updated to remove `DoorActionParams` from the variant)
- `ActionEntry` struct

**Moves to door module** (`DoorActionTypes.h`):
- `DoorActionParams`
- The three `ActionType` enum values for doors

**Complication:** `ActionParams` is a `std::variant` that currently includes `DoorActionParams`. Removing it means the variant type changes, breaking any code that `std::get_if<DoorActionParams>()`. The cleanest solution is to keep `DoorActionParams` in `ActionTypes.h` as a forward type included from the door module, OR keep it in ActionTypes.h and just move the handling to the door module. Per D-08, the type should move — but this requires the `ActionParams` variant to be extensible, which a fixed-type `std::variant` is not.

**Practical resolution:** Move `DoorActionParams` to `DoorActionTypes.h`, and have `ActionTypes.h` `#include "game/modules/door/DoorActionTypes.h"` to retain the variant member. This satisfies the intent (door types are owned by the module) while avoiding a breaking change to `ActionParams`. The planner should decide whether this counts as "ActionTypes.h including a module header" (a small layer violation) or accept it as transitional.

### Duplicate Action Dispatch — The Real Split Brain

`BehaviorSystem::executeAction()` (in `behavior/BehaviorSystem.cpp`) and `updateRuntimeBehaviors()` (in `runtime/RuntimeGameplay.cpp`) both implement the same ToggleDoor/OpenDoor/CloseDoor logic. The code is nearly identical. After Phase 21:

- Both must be replaced by `registeredHandler(app/registry, source, action)` calls
- The door logic lives once in `DoorActionHandler.cpp`
- BehaviorSystem session uses `Application&`-based dispatch
- RuntimeGameplay session uses `entt::registry&`-based dispatch

The registration API must accommodate both call signatures, OR the door handler always takes `entt::registry&` and BehaviorSystem extracts `app.registry()` before calling.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead |
|---------|-------------|-------------|
| Extensible parser keywords | Custom plugin framework | `std::unordered_map<string, std::function>` (simple, already used for NodeIndex pattern) |
| CMake conditional compilation | `#ifdef EDITOR` in source | Separate `target_sources()` per CMake target |
| Action dispatch extensibility | Virtual base class hierarchy | `std::unordered_map<ActionType, std::function>` registration |

---

## Common Pitfalls

### Pitfall 1: Circular Dependency — `game_module_door` ↔ `gameplay`
**What goes wrong:** `DoorSpawner.cpp` includes `LevelBuilder.h`. `LevelBuilder.h` is in `gameplay`. If `gameplay` also links `game_module_door`, you have a circular CMake dependency that fails to link.
**Why it happens:** `addDoorGroup()` currently lives in LevelBuilder and calls builder methods. Moving it to DoorSpawner means DoorSpawner uses LevelBuilder as a parameter type.
**How to avoid:** `DoorSpawner` takes `LevelBuilder&` as a parameter (fine — it includes LevelBuilder.h but doesn't link gameplay as a library target). `gameplay` removes its door code but does NOT link `game_module_door`. `game_module_door` links `gameplay` as a PUBLIC dep.
**Warning signs:** CMake link error mentioning circular dependency or undefined references in both directions.

### Pitfall 2: Double Registration if Called Twice
**What goes wrong:** `registerDoorModule()` called from both `main.cpp` and from some initialization in `RuntimeGameSession`, resulting in duplicate handlers in the map.
**Why it happens:** RuntimeGameSession is designed to be instantiated multiple times (editor preview + runtime). If registration is mixed into RuntimeGameSession::rebuild(), it runs on every scene reload.
**How to avoid:** Registration must happen once at application startup (inside `Application::init()` or equivalent), not per-session. Use a static bool guard or assert-once pattern if defensive programming is desired.

### Pitfall 3: `LevelDef::doors` Vector Orphaned After `door_group` Parser Removed
**What goes wrong:** After extracting the parser, the `door_group` keyword is no longer recognized. Scene files fail to load. Existing tests that use `resolveLevelHierarchy` on LevelDef structs still pass (they don't need the parser), but round-trip tests fail.
**Why it happens:** `test_level_roundtrip.cpp` and `test_door_group_position.cpp` would fail if the parser is removed before being replaced.
**How to avoid:** The parser replacement (registration call) must happen before removing the inline `door_group` block from `LevelDef.cpp`. Wave ordering matters: register the new callback first, verify tests pass, then delete the old inline code.

### Pitfall 4: SceneSelectionInspector Hard-Codes DoorGroupInspector Include Path
**What goes wrong:** `SceneSelectionInspector.cpp` has `#include "editor/ui/inspectors/DoorGroupInspector.h"` at line 10 (verified). After moving the file, this path is invalid.
**Why it happens:** Include paths must be updated at all call sites when files move.
**How to avoid:** Update `SceneSelectionInspector.cpp` to `#include "game/modules/door/editor/DoorGroupInspector.h"`. Only one caller to update.

### Pitfall 5: `EditorPreviewWorld.cpp` Creates `DoorConfigComponent` Directly
**What goes wrong:** Line 243 of `EditorPreviewWorld.cpp` directly instantiates `DoorConfigComponent` without going through `DoorSpawner`. After the component headers move, the include path breaks.
**Why it happens:** EditorPreviewWorld uses a simplified spawn path (no leaf linking in editor preview). This is intentional and should remain separate from DoorSpawner.
**How to avoid:** Update the include path in EditorPreviewWorld.cpp. The logic doesn't need to change — it's a legitimate simplified spawn that creates only the root config/state for animation preview, not the full spawner.

### Pitfall 6: `RuntimeGameSession.cpp` Uses `tickDoorAnimation` Free Function
**What goes wrong:** Line 213 calls `tickDoorAnimation(registry_, deltaTime)`. After header move, include path breaks. Easy to miss in a file-move sweep.
**How to avoid:** Include path in RuntimeGameSession.cpp updates from `game/behavior/DoorAnimationSystem.h` to `game/modules/door/DoorAnimationSystem.h`.

---

## Code Examples

### Verified: Current `addDoorGroup` Signature (to extract into DoorSpawner)
```cpp
// Source: src/game/level/LevelBuilder.cpp line 283
// Current signature — DoorSpawner will provide a standalone function with same parameters:
entt::entity LevelBuilder::addDoorGroup(const LevelDoorPlacement& group, const LevelDef& level);

// After extraction, DoorSpawner.h exposes:
entt::entity spawnDoorGroup(LevelBuilder& builder,
                             const LevelDoorPlacement& group,
                             const LevelDef& level);
// LevelBuilder::addDoorGroup becomes a thin wrapper that calls spawnDoorGroup(...)
// or is removed and all callers (LevelLoader) call spawnDoorGroup directly.
```

### Verified: Current `door_group` Parser Block Location
```cpp
// Source: src/game/level/LevelDef.cpp lines 971-1018
// The if (kind == "door_group") block is the extraction target for DoorSerializer.cpp
// Parser helper functions it uses (tryParseFloatToken, parseNodeMetadata, collectRemainingTokens)
// live in anonymous namespace in LevelDef.cpp — they must either stay there (and be used
// via the callback's closure) or be moved to a ParseUtils header.
```

### Verified: Current Serializer Location
```cpp
// Source: src/game/level/LevelDef.cpp lines 1422-1435
// for (const auto& dg : data.doors) { out << "door_group" ... }
// This block becomes serializeDoorGroups(out, data) in DoorSerializer.cpp
```

### Verified: BehaviorSystem Action Dispatch (to be replaced by handler map)
```cpp
// Source: src/game/behavior/BehaviorSystem.cpp line 170
switch (action.type) {
case ActionType::OpenDoor:  { /* 24 lines */ break; }
case ActionType::CloseDoor: { /* 12 lines */ break; }
case ActionType::ToggleDoor:{ /* 28 lines */ break; }
// ... 11 more non-door cases ...
}
// After: look up registered handler, call it
```

### Verified: Duplicate in RuntimeGameplay
```cpp
// Source: src/game/runtime/RuntimeGameplay.cpp line 450
// IDENTICAL logic to BehaviorSystem for ToggleDoor/OpenDoor/CloseDoor
// This is the split brain. Both paths must call the same DoorActionHandler.
```

### Verified: Test That Exercises Door Parser
```cpp
// Source: tests/game/test_door_group_position.cpp
// Uses: makePivotLeafModel (from GameplayPrefabs), resolveLevelHierarchy (from LevelDef)
// After: include paths update to game/modules/door/DoorMath.h
// Links: test currently links `gameplay` — after refactor links `game_module_door`
```

---

## File Move Manifest

Complete list of changes with source and destination:

### Files that MOVE (new path, delete old):
| Source | Destination |
|--------|-------------|
| `src/game/components/DoorConfigComponent.h` | `src/game/modules/door/DoorConfigComponent.h` (or merged into DoorComponents.h) |
| `src/game/components/DoorStateComponent.h` | `src/game/modules/door/DoorStateComponent.h` |
| `src/game/components/DoorLeafComponent.h` | `src/game/modules/door/DoorLeafComponent.h` |
| `src/game/behavior/DoorAnimationSystem.h` | `src/game/modules/door/DoorAnimationSystem.h` |
| `src/game/behavior/DoorAnimationSystem.cpp` | `src/game/modules/door/DoorAnimationSystem.cpp` |
| `src/editor/ui/inspectors/DoorGroupInspector.h` | `src/game/modules/door/editor/DoorGroupInspector.h` |
| `src/editor/ui/inspectors/DoorGroupInspector.cpp` | `src/game/modules/door/editor/DoorGroupInspector.cpp` |

### Files that get NEW content extracted FROM them:
| Source File | Extracted Into |
|-------------|----------------|
| `src/game/level/LevelBuilder.cpp` (addDoorGroup method) | `src/game/modules/door/DoorSpawner.cpp` |
| `src/game/level/LevelDef.cpp` (door_group parser + serializer) | `src/game/modules/door/DoorSerializer.cpp` |
| `src/game/prefabs/GameplayPrefabs.h/.cpp` (makePivotLeafModel) | `src/game/modules/door/DoorMath.h/.cpp` |
| `src/game/behavior/ActionTypes.h` (DoorActionParams + door ActionType entries) | `src/game/modules/door/DoorActionTypes.h` |
| `src/game/behavior/BehaviorSystem.cpp` (door cases in executeAction) | `src/game/modules/door/DoorActionHandler.cpp` |
| `src/game/runtime/RuntimeGameplay.cpp` (door cases in updateRuntimeBehaviors) | `src/game/modules/door/DoorActionHandler.cpp` (same file) |

### Files that need INCLUDE PATH UPDATES only:
| File | Change Needed |
|------|---------------|
| `src/editor/ui/inspectors/SceneSelectionInspector.cpp` | Update DoorGroupInspector include path |
| `src/editor/scene/EditorPreviewWorld.cpp` | Update DoorConfigComponent, DoorStateComponent include paths |
| `src/game/runtime/RuntimeGameSession.cpp` | Update DoorAnimationSystem, DoorConfigComponent, DoorStateComponent includes |
| `src/game/level/LevelLoader.cpp` | No door includes directly, but LevelDef.h may change |
| `tests/game/test_door_group_position.cpp` | Update makePivotLeafModel include path |

### New Files Created:
| File | Purpose |
|------|---------|
| `src/game/modules/door/DoorMath.h/.cpp` | makePivotLeafModel |
| `src/game/modules/door/DoorSpawner.h/.cpp` | spawnDoorGroup extracted from LevelBuilder |
| `src/game/modules/door/DoorSerializer.h/.cpp` | parse/serialize door_group keyword |
| `src/game/modules/door/DoorActionHandler.h/.cpp` | door action execution (merged BehaviorSystem + RuntimeGameplay door logic) |
| `src/game/modules/door/DoorActionTypes.h` | DoorActionParams, door ActionType values |
| `src/game/modules/door/DoorModule.h/.cpp` | registerDoorModule() entry point |
| `src/game/modules/door/CMakeLists.txt` | game_module_door CMake target |
| `tests/game/modules/door/test_door_group_position.cpp` | Moved test |
| `tests/game/modules/door/CMakeLists.txt` | Test registration for moved test |

---

## Registration Protocol Design Options

Claude's Discretion covers the exact API. Two viable designs:

### Option A: Global Registration Functions (Simplest)
```cpp
// LevelDef.h additions:
void registerLevelDefKeyword(const std::string& keyword,
    std::function<void(LevelDef&, const std::string& path, int line,
                       const std::vector<std::string>& tokens)> parser,
    std::function<void(std::ostringstream&, const LevelDef&)> serializer);
```
The registry lives in a file-scoped `std::unordered_map` inside `LevelDef.cpp`. No new class needed.

### Option B: Registry Object Passed to registerDoorModule
```cpp
// ParserRegistry.h:
class LevelDefRegistry {
public:
    void registerKeyword(std::string keyword, ParseCallback, SerializeCallback);
    // loadLevelDef and serializeLevelDef use this to dispatch
};
```
LevelDef.cpp holds a singleton or the Application holds an instance passed to registrations.

Option A is simpler and matches the existing pattern of `NodeIndex` and `InteractionFocusState` being stored in `registry.ctx()`. Option B is more explicit about ownership. Both satisfy D-05 through D-07.

**Recommendation:** Option A. Less boilerplate, consistent with the existing pattern of file-scoped registration in this codebase.

---

## Test Impact

| Test File | Current Location | Impact | Action |
|-----------|-----------------|--------|--------|
| `test_door_group_position.cpp` | `tests/game/` | Links `gameplay`; uses `makePivotLeafModel` and `resolveLevelHierarchy` | Move to `tests/game/modules/door/`; update includes; link `game_module_door` instead of `gameplay` |
| `test_level_roundtrip.cpp` | `tests/game/` | Exercises door_group round-trip via parser | Links `game_content` — must link `game_module_door` after parser extraction |
| `test_level_def.cpp` | `tests/game/` | Reads cathedral.scene which contains door_group records | Same: needs parser registered; likely requires linking `game_module_door` and calling `registerDoorModule()` before `loadLevelDef()` |
| `test_behavior_trigger_roundtrip.cpp` | `tests/game/` | May test door action serialization | Check if it exercises door actions; add `game_module_door` link if so |

All door-related tests must call `registerDoorModule()` before calling `loadLevelDef()` for tests that exercise scene parsing. This is the key test setup change.

---

## Validation Architecture

Validation skipped per `workflow.nyquist_validation: false` in `.planning/config.json`.

Manual build verification command (run after each wave):
```bash
cmake --build build --target pixel-roguelike level-editor procedural-model-viewer
cd build && ctest --output-on-failure -L game
```

---

## Open Questions

1. **`LevelDoorPlacement` ownership**
   - What we know: Defined in `LevelDef.h`, used by EditorSceneDocument, EditorSceneSerializer, SceneSelectionInspector, EditorPreviewWorld
   - What's unclear: Moving it to the door module makes LevelDef.h include from `game/modules/door/`, blurring the boundary; keeping it in LevelDef.h keeps the layer clean but means door module depends on LevelDef.h (acceptable since DoorSerializer already operates on LevelDef)
   - Recommendation: Keep `LevelDoorPlacement` in `LevelDef.h` for Phase 21. The struct is part of the level data format, not door behavior.

2. **`ActionType` enum extensibility**
   - What we know: `ActionType` is a `uint8_t` enum in `ActionTypes.h`; `ActionParams` variant includes `DoorActionParams`
   - What's unclear: Removing door entries from the `ActionType` enum changes the enum values (if they're in the middle) and breaks persisted scene files that store action types by integer
   - Recommendation: Keep `ActionType::OpenDoor/CloseDoor/ToggleDoor` enum values in `ActionTypes.h` but move `DoorActionParams` to `DoorActionTypes.h`. The enum values are shared interface; the params struct is implementation detail.

3. **EditorPreviewWorld door spawn path**
   - What we know: Lines 230-253 create DoorConfigComponent directly without calling addDoorGroup — this is intentional (simplified editor preview)
   - What's unclear: Should this path use DoorSpawner? It deliberately omits leaf entity linking.
   - Recommendation: Leave EditorPreviewWorld's door handling unchanged (just update include paths). It's not a bug and DoorSpawner would add complexity to the simplified path.

---

## Sources

### Primary (HIGH confidence — verified by direct source inspection)
- `src/game/components/DoorConfigComponent.h` — component fields confirmed
- `src/game/components/DoorStateComponent.h` — state machine helpers confirmed
- `src/game/components/DoorLeafComponent.h` — leaf animation data confirmed
- `src/game/behavior/DoorAnimationSystem.h/.cpp` — tickDoorAnimation free function confirmed
- `src/game/behavior/ActionTypes.h` — DoorActionParams in ActionParams variant confirmed
- `src/game/behavior/BehaviorSystem.cpp` — door action switch at line 170 confirmed
- `src/game/level/LevelBuilder.cpp` — addDoorGroup at line 283, 116 lines of logic confirmed
- `src/game/level/LevelDef.cpp` — door_group parser at line 971, serializer at line 1422 confirmed
- `src/game/prefabs/GameplayPrefabs.h/.cpp` — makePivotLeafModel confirmed
- `src/editor/ui/inspectors/DoorGroupInspector.h/.cpp` — standalone, ready to move
- `src/editor/scene/EditorPreviewWorld.cpp` — DoorGroup case at line 230, direct component creation
- `src/game/runtime/RuntimeGameplay.cpp` — duplicate door action switch at line 450
- `src/game/runtime/RuntimeGameSession.cpp` — tickDoorAnimation call at line 213
- `src/editor/ui/inspectors/SceneSelectionInspector.cpp` — DoorGroupInspector include at line 10
- `src/game/CMakeLists.txt` — gameplay target sources confirmed
- `src/editor/CMakeLists.txt` — editor target sources confirmed
- `tests/game/CMakeLists.txt` — test registrations confirmed
- `tests/game/test_door_group_position.cpp` — links gameplay, confirmed

---

## Metadata

**Confidence breakdown:**
- File inventory: HIGH — every file verified by direct source inspection
- CMake structure: HIGH — both CMakeLists.txt files read in full
- Registration protocol: MEDIUM — design pattern is established in codebase (NodeIndex, ctx registry), but exact API is Claude's Discretion
- Test impact: HIGH — test files read, link targets confirmed
- Pitfalls: HIGH — all identified from direct code inspection, not assumptions

**Research date:** 2026-04-07
**Valid until:** Stable until Phase 20 modifies the door system further (check for Phase 20 completion before executing)
