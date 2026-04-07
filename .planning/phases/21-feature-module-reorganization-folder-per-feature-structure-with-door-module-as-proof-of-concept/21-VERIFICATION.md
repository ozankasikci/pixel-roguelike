---
phase: 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept
verified: 2026-04-08T00:00:00Z
status: gaps_found
score: 4/7 must-haves verified
overrides_applied: 0
gaps:
  - truth: "All door-related code lives in src/game/modules/door/ — no door logic remains in components/, systems/, prefabs/, behavior/, or scattered editor files"
    status: failed
    reason: "Substantial door logic remains outside the module. LevelDef.h still contains LevelDoorPlacement struct and doors field. LevelDef.cpp still contains parseDoorActionParams(), DoorGroup enum value in the LevelNodeRef::Kind enum, and data.doors references throughout resolveLevelHierarchy(). EditorSceneDocument.h has DoorGroup hardcoded in EditorSceneObjectKind enum, LevelDoorPlacement in the EditorSceneObjectPayload variant, and addDoorGroup() method. EditorCommand.cpp, EditorViewportInteraction.cpp, EditorScenePreviewRenderer.cpp, and InspectorUtils.cpp all have door-specific switch cases and logic."
    artifacts:
      - path: "src/game/level/LevelDef.h"
        issue: "LevelDoorPlacement struct (lines 113-125) and doors vector in LevelDef struct (line 138) not moved to module"
      - path: "src/game/level/LevelDef.cpp"
        issue: "parseDoorActionParams() function (lines 195-207), DoorGroup in LevelNodeRef::Kind enum (line 160), data.doors references in resolveLevelHierarchy (lines 1014, 1038-1039, 1090-1092, 1196-1203)"
      - path: "src/editor/scene/EditorSceneDocument.h"
        issue: "DoorGroup in EditorSceneObjectKind enum (line 22), LevelDoorPlacement in EditorSceneObjectPayload variant (line 33), addDoorGroup() method declaration (line 82)"
      - path: "src/editor/scene/EditorSceneDocument.cpp"
        issue: "addDoorGroup() implementation, level.doors.push_back, LevelDoorPlacement-specific branches in serialize/deserialize"
      - path: "src/editor/ui/inspectors/InspectorUtils.cpp"
        issue: "Door action type UI labels and DoorActionParams initialization at lines 26, 56-59, 351-353"
      - path: "src/editor/viewport/EditorViewportInteraction.cpp"
        issue: "DoorGroup case handling in 4+ switch statements"
      - path: "src/editor/core/EditorCommand.cpp"
        issue: "DoorGroup case that pushes to level.doors at line 45"
      - path: "src/editor/render/EditorScenePreviewRenderer.cpp"
        issue: "DoorGroup pivot marker rendering at lines 308-309"
    missing:
      - "LevelDoorPlacement struct should move to src/game/modules/door/ (e.g. DoorComponents.h or a new DoorLevelTypes.h)"
      - "parseDoorActionParams() should move to DoorSerializer.cpp or DoorActionTypes module"
      - "LevelNodeRef::Kind::DoorGroup should be removed or made extensible via module registration"
      - "EditorSceneObjectKind::DoorGroup should be removed or made extensible"
      - "EditorSceneObjectPayload variant should not hardcode LevelDoorPlacement"
      - "InspectorUtils.cpp door action UI should move to door module editor/ directory"

  - truth: "Scene serialization for doors is owned by the door module"
    status: failed
    reason: "The door_group keyword dispatch in LevelDef.cpp is now registered via DoorSerializer (correct). However LevelDef.cpp still contains parseDoorActionParams() for open_door/close_door/toggle_door action entries in the behavior trigger system — these are not dispatched via the module registration. EditorSceneDocument.cpp still has door-specific serialization branches (LevelDoorPlacement in toLevelDef mapping). The PLAN said DoorSerializer owned all door serialization but action param parsing in LevelDef.cpp was not addressed."
    artifacts:
      - path: "src/game/level/LevelDef.cpp"
        issue: "parseDoorActionParams() at line 195 and its callers at lines 365-368, 429-432, 542-544, 573 handle door action type serialization inside LevelDef, not inside the module"
      - path: "src/editor/scene/EditorSceneDocument.cpp"
        issue: "LevelDoorPlacement-specific branches in toLevelDef and fromLevelDef at lines 605, 675, 815, 889"
    missing:
      - "parseDoorActionParams() and related action serialization should be owned by the door module"
      - "EditorSceneDocument door serialization should be handled via module-registered callbacks"

  - truth: "Adding a hypothetical new interactive object would mean creating a new src/game/modules/name/ directory without touching core systems"
    status: failed
    reason: "A new module would still require: (1) adding a new enum value to EditorSceneObjectKind in EditorSceneDocument.h, (2) adding the placement struct to the EditorSceneObjectPayload variant in EditorSceneDocument.h, (3) adding handling in EditorCommand.cpp, EditorViewportInteraction.cpp, EditorScenePreviewRenderer.cpp switch statements, (4) adding action type enum values to ActionTypes.h, and (5) adding action param parsing to LevelDef.cpp. These are all core-system modifications. The door module as implemented is not yet a clean proof of concept of the folder-per-feature extensibility goal."
    artifacts:
      - path: "src/editor/scene/EditorSceneDocument.h"
        issue: "EditorSceneObjectKind enum and EditorSceneObjectPayload variant must be edited to add a new interactive object kind"
      - path: "src/game/behavior/ActionTypes.h"
        issue: "ActionType enum must be edited to add new action types for a new interactive object"
      - path: "src/game/level/LevelDef.cpp"
        issue: "parseDoorActionParams pattern requires new per-action-type parser function here for new objects"
    missing:
      - "EditorSceneObjectKind needs an extensible registration mechanism (or the door-specific value should be registered, not hardcoded)"
      - "EditorSceneObjectPayload variant would need to be extensible (e.g. type-erased polymorphic approach, or the placement struct moved to a module-agnostic shared type)"
      - "ActionType enum extensibility would need a registered/dynamic approach for new modules to add action types without editing ActionTypes.h"
---

# Phase 21: Feature Module Reorganization Verification Report

**Phase Goal:** Reorganize the codebase from type-based organization to feature-based modules. Consolidate the door system's 34-file, 7-layer spread into a single self-contained directory. Eliminate duplicated spawn paths, state machine implementations, and pivot math. Establish the folder-per-feature pattern that future interactive objects will follow.
**Verified:** 2026-04-08
**Status:** gaps_found
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | All door-related code lives in src/game/modules/door/ | FAILED | LevelDef.h has LevelDoorPlacement + doors field; LevelDef.cpp has parseDoorActionParams() and DoorGroup node handling; EditorSceneDocument.h has DoorGroup enum + LevelDoorPlacement variant; 4+ editor files have door switch cases |
| 2 | Door spawning has a single implementation (not duplicated) | VERIFIED | LevelLoader.cpp calls spawnDoorGroup(builder, doorGroup, level). LevelBuilder::addDoorGroup() is deleted. No duplicate spawn paths found. |
| 3 | Door animation/state machine has a single implementation | VERIFIED | BehaviorSystem.cpp dispatches OpenDoor/CloseDoor/ToggleDoor via findBehaviorActionHandler(). RuntimeGameplay.cpp uses same dispatch (line 449). No inline door component access in either file. |
| 4 | Editor inspector for doors is part of the door module | VERIFIED | DoorGroupInspector.h/.cpp lives in src/game/modules/door/editor/. SceneSelectionInspector.cpp includes from module path. editor CMakeLists.txt compiles from module path. |
| 5 | Scene serialization for doors is owned by the door module | FAILED | door_group keyword dispatch is registered via DoorSerializer (correct). But parseDoorActionParams() for open_door/close_door/toggle_door actions remains in LevelDef.cpp (not moved to module). EditorSceneDocument.cpp has door-specific toLevelDef/fromLevelDef branches. |
| 6 | All existing tests pass, both main executables build | VERIFIED | 13/13 game tests pass. pixel-roguelike and level-editor build successfully. procedural-model-viewer is behind BUILD_MODEL_VIEWER=OFF flag (opt-in, not a regression). |
| 7 | Adding a new interactive object means creating a new modules/name/ directory without touching core systems | FAILED | Would still require editing EditorSceneObjectKind enum, EditorSceneObjectPayload variant, ActionTypes.h enum, LevelDef.cpp action parsers, and multiple editor switch statements — all core-system files. |

**Score:** 4/7 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/modules/door/DoorComponents.h` | DoorConfigComponent, DoorStateComponent, DoorLeafComponent | VERIFIED | Contains all three structs + helper free functions |
| `src/game/modules/door/DoorAnimationSystem.h` | DoorAnimationSystem class + tickDoorAnimation | VERIFIED | Both present with correct signatures |
| `src/game/modules/door/DoorMath.h` | makePivotLeafModel declaration | VERIFIED | Present |
| `src/game/modules/door/DoorSpawner.h` | spawnDoorGroup function | VERIFIED | Present; wired in LevelLoader.cpp |
| `src/game/modules/door/DoorActionTypes.h` | DoorActionParams struct | VERIFIED | Present |
| `src/game/modules/door/DoorSerializer.h` | parseDoorGroup and serializeDoorGroups | VERIFIED | Present and registered via DoorModule |
| `src/game/modules/door/DoorActionHandler.h` | handleDoorAction function | VERIFIED | Present and registered via DoorModule |
| `src/game/modules/door/DoorModule.h` | registerDoorModule entry point | VERIFIED | Present; called in both app main() functions |
| `src/game/modules/door/CMakeLists.txt` | game_module_door static library target | VERIFIED | Compiles all 6 cpp files, links gameplay PUBLIC |
| `src/game/modules/door/editor/DoorGroupInspector.h` | drawDoorGroupInspector | VERIFIED | Present in module editor subdirectory |
| `src/game/level/LevelDef.h` | registerLevelDefKeyword() API | VERIFIED | LevelDefParseCallback, LevelDefSerializeCallback, registerLevelDefKeyword() all present |
| `src/game/behavior/BehaviorSystem.h` | registerBehaviorActionHandler() API | VERIFIED | BehaviorActionHandler, registerBehaviorActionHandler(), findBehaviorActionHandler() all present |
| `apps/runtime/main.cpp` | registerDoorModule() call at startup | VERIFIED | Line 89, after content.loadDefaults() |
| `apps/level_editor/main.cpp` | registerDoorModule() call at startup | VERIFIED | Line 329, after content.loadDefaults() |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| src/game/modules/door/CMakeLists.txt | src/game/CMakeLists.txt | add_subdirectory(modules/door) | WIRED | Line 65 of game/CMakeLists.txt |
| src/game/modules/door/DoorModule.cpp | src/game/level/LevelDef.h | registerLevelDefKeyword call | WIRED | Registers "door_group" keyword |
| src/game/modules/door/DoorModule.cpp | src/game/behavior/BehaviorSystem.h | registerBehaviorActionHandler call | WIRED | Registers OpenDoor/CloseDoor/ToggleDoor |
| apps/runtime/main.cpp | src/game/modules/door/DoorModule.h | include and function call | WIRED | Line 11 include, line 89 call |
| src/game/level/LevelLoader.cpp | src/game/modules/door/DoorSpawner.h | spawnDoorGroup call | WIRED | Line 91: spawnDoorGroup(builder, doorGroup, level) |
| src/editor/ui/inspectors/SceneSelectionInspector.cpp | src/game/modules/door/editor/DoorGroupInspector.h | include path | WIRED | Line 10 |
| src/editor/CMakeLists.txt | src/game/modules/door/editor/DoorGroupInspector.cpp | source compilation | WIRED | Line 33 |

### Data-Flow Trace (Level 4)

Not applicable — this phase is a code reorganization, not a data-rendering feature.

### Behavioral Spot-Checks

| Behavior | Result | Status |
|----------|--------|--------|
| All 13 game tests pass | 13/13 passed via ctest -L game | PASS |
| pixel-roguelike builds | cmake --build build --target pixel-roguelike succeeded | PASS |
| level-editor builds | cmake --build build --target level-editor succeeded | PASS |
| DoorModule registers door_group keyword | LevelDef.cpp dispatches via keywordRegistry().find(kind); no inline door_group parser | PASS |
| BehaviorSystem dispatches via handler registry | Lines 189-196 of BehaviorSystem.cpp: findBehaviorActionHandler(action.type) | PASS |
| RuntimeGameplay has no inline door switch | Only line 449 (findBehaviorActionHandler dispatch) found in updateRuntimeBehaviors | PASS |

### Requirements Coverage

No REQUIREMENTS.md IDs were claimed for this architectural phase.

### Anti-Patterns Found

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| src/game/level/LevelDef.h | LevelDoorPlacement struct (lines 113-125) + doors vector (line 138) — door data type embedded in core level data struct | Blocker | Core level struct owns door data; modules cannot be independent without owning their LevelDef types |
| src/game/level/LevelDef.cpp | parseDoorActionParams() (line 195) — door action param parsing not moved to module | Blocker | Any new action-type module must add a parseXxxActionParams() to LevelDef.cpp |
| src/editor/scene/EditorSceneDocument.h | DoorGroup in EditorSceneObjectKind enum (line 22); LevelDoorPlacement in payload variant (line 33) | Blocker | New module would require editing this core editor type — violates the phase goal |
| src/editor/ui/inspectors/InspectorUtils.cpp | Door action UI (lines 26, 56-59) not in door module editor/ | Warning | Not in module but small in scope |
| LevelDef.cpp (lines 365-368, 542-544, 573) | Door-specific ActionType switch cases for param init and serialization | Warning | Action serialization not owned by module |

### Human Verification Required

None — structural/wiring gaps can be confirmed programmatically.

### Gaps Summary

Phase 21 established the mechanical foundation of the feature-module pattern — the `src/game/modules/door/` directory exists with all 16 source files, `game_module_door` compiles, the registration APIs work, and the two main executables build with all 13 tests passing. The spawning deduplication (SC2) and state machine consolidation (SC3) are fully achieved.

However, the phase did not achieve its stated goal of **all** door code in the module. Three categories of door logic were not moved:

**Category 1: LevelDef data ownership.** `LevelDoorPlacement` and `LevelDef::doors` remain in `LevelDef.h/.cpp`. The node hierarchy resolution code in `LevelDef.cpp` has door-specific `DoorGroup` handling baked into the core `LevelNodeRef::Kind` enum and `resolveLevelHierarchy()`. The action entry parser has a `parseDoorActionParams()` function for `open_door`/`close_door`/`toggle_door` action types. These were not migrated because they are tightly coupled to the core LevelDef data model.

**Category 2: Editor scene document.** `EditorSceneDocument.h` hardcodes `DoorGroup` in `EditorSceneObjectKind` and `LevelDoorPlacement` in the payload variant. Five editor files (`EditorSceneDocument.cpp`, `EditorCommand.cpp`, `EditorViewportInteraction.cpp`, `EditorScenePreviewRenderer.cpp`, `InspectorUtils.cpp`) have door-specific switch cases. The plan only moved `DoorGroupInspector` to the module but did not address these editor core files.

**Category 3: SC7 extensibility not demonstrated.** Because the editor scene graph and LevelDef data types remain door-specific and non-extensible, the proof-of-concept does not demonstrate that adding a hypothetical lever module would only require creating `src/game/modules/lever/`. The remaining door entrenchment in core types means new modules would still require editing `EditorSceneObjectKind`, `ActionTypes.h`, `LevelDef.h`, and `LevelDef.cpp`.

These are architectural root-cause issues, not simple file moves. The gaps are likely larger in scope than the original three plans anticipated.

---

_Verified: 2026-04-08_
_Verifier: Claude (gsd-verifier)_
