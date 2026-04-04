---
phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity
plan: 01
subsystem: editor
tags: [cpp, editor, triggers, behaviors, ecs, imgui, serialization, roundtrip]

# Dependency graph
requires:
  - phase: 13-data-driven-behavior-system
    provides: TriggerPlacement, BehaviorDeclaration, ActionTypes in LevelDef
provides:
  - EditorSceneObjectKind::Trigger as first-class variant
  - addTrigger() method on EditorSceneDocument
  - Full trigger save/load round-trip in editor
  - "Add Trigger Zone" outliner context menu with undo
  - showTriggers viewport toggle in EditorUiState
  - Trigger inspector panel with shape/halfExtents/radius/fireOnce fields
  - Behavior/trigger fidelity test suite
affects:
  - 16-02 (trigger inspector behaviors panel)
  - 16-03 (behavior authoring full UX)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - EditorSceneObjectKind extension: add enum value, variant type, all switch sites
    - TDD round-trip test pattern: serialize -> save -> load -> assert field equality
    - appendHelperObjects bool parameter pattern for viewport visibility toggles

key-files:
  created:
    - tests/game/test_behavior_trigger_roundtrip.cpp
  modified:
    - src/editor/scene/EditorSceneDocument.h
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/core/EditorCommand.cpp
    - src/editor/render/EditorScenePreviewRenderer.h
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/ui/EditorOutlinerPanel.cpp
    - src/editor/ui/LevelEditorUi.h
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - apps/level_editor/main.cpp
    - tests/game/CMakeLists.txt

key-decisions:
  - "Trigger objects use addObject() generic path — no special-casing like PlayerSpawn"
  - "showTriggers defaults to true with default parameter in appendHelperObjects signature"
  - "Trigger inspector shows shape/halfExtents/radius/fireOnce only (behaviors in 16-02)"
  - "EditorViewportInteraction Trigger cases fall through to same applyWorldTransform path as Mesh"

patterns-established:
  - "Pattern 1: Add enum value + variant + all switch sites atomically — no dangling cases"
  - "Pattern 2: Round-trip test uses test_support::tempPath() and nearlyEqual for float assertions"

requirements-completed: []

# Metrics
duration: 60min
completed: 2026-04-04
---

# Phase 16 Plan 01: Trigger Promotion to First-Class Editor Objects Summary

**Triggers promoted from read-only side-channel to full EditorSceneObject entries with kind, variant, outliner display, context menu creation, inspector, and serialization round-trip**

## Performance

- **Duration:** ~60 min
- **Started:** 2026-04-04T18:30:00Z
- **Completed:** 2026-04-04T19:27:49Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments
- `EditorSceneObjectKind::Trigger` added with `TriggerPlacement` in the variant — all switch/visitor sites updated atomically
- `readOnlyTriggers_` member and `triggers()` accessor fully removed; `addTrigger()` method added
- `loadFromSceneFile()` now promotes triggers via `addTrigger()` so they participate in undo/redo and serialization
- `toLevelDef()` serializes Trigger objects back to `level.triggers`
- "Add Trigger Zone" right-click context menu in outliner creates a trigger at the entity's position with full undo support
- `showTriggers = true` toggle added to `EditorUiState`; trigger wireframe rendering gated on it
- Trigger inspector shows position, shape combo, halfExtents/radius, and fireOnce checkbox
- Round-trip fidelity test passes for box triggers, sphere triggers, mesh interactables, and light behaviors

## Task Commits

Each task was committed atomically:

1. **Task 1: Add Trigger to EditorSceneObjectKind and update all switch/visitor sites** - `f42c4ac` (feat)
2. **Task 2: Add outliner context menu "Add Trigger Zone" and showTriggers toggle** - `c34a2c8` (feat)
3. **Task 3: Create dedicated behavior/trigger round-trip fidelity test** - `b688a2c` (test)

## Files Created/Modified
- `src/editor/scene/EditorSceneDocument.h` - Added Trigger enum value, TriggerPlacement variant, addTrigger(), removed readOnlyTriggers_
- `src/editor/scene/EditorSceneDocument.cpp` - All switch/visitor sites updated; loadFromSceneFile/toLevelDef handling
- `src/editor/core/EditorCommand.cpp` - Added Trigger case to makeLevelDefFromState switch
- `src/editor/render/EditorScenePreviewRenderer.h` - Added showTriggers parameter to appendHelperObjects
- `src/editor/render/EditorScenePreviewRenderer.cpp` - Updated trigger rendering from objects() with selection highlight and showTriggers guard
- `src/editor/ui/EditorOutlinerPanel.cpp` - Added "Add Trigger Zone" context menu item
- `src/editor/ui/LevelEditorUi.h` - Added showTriggers = true to EditorUiState
- `src/editor/ui/EditorInspectorPanel.cpp` - Added Trigger inspector case with shape/halfExtents/radius/fireOnce
- `src/editor/viewport/EditorViewportInteraction.cpp` - Added Trigger to gizmo interaction switches
- `apps/level_editor/main.cpp` - Threaded ui.showTriggers through appendHelperObjects call
- `tests/game/test_behavior_trigger_roundtrip.cpp` - New round-trip fidelity test (5 test cases)
- `tests/game/CMakeLists.txt` - Registered test_behavior_trigger_roundtrip

## Decisions Made
- Triggers use the generic `addObject()` path — no special-casing needed unlike PlayerSpawn (which deduplicates)
- `showTriggers` has a default value of `true` in the `appendHelperObjects` signature so existing callers don't break
- Trigger inspector limited to geometric/metadata fields; behavior authoring deferred to 16-02
- `EditorViewportInteraction.cpp` gizmo switches fall Trigger through the same `applyWorldTransform` path as Mesh/Group

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed EditorCommand.cpp missing Trigger case in switch**
- **Found during:** Task 1 (build verification)
- **Issue:** `makeLevelDefFromState()` switch in EditorCommand.cpp had no Trigger case — compiler warning, missing round-trip in undo/redo history
- **Fix:** Added `case EditorSceneObjectKind::Trigger: level.triggers.push_back(...); break;`
- **Files modified:** `src/editor/core/EditorCommand.cpp`
- **Verification:** No warning in subsequent build
- **Committed in:** `f42c4ac` (Task 1 commit)

**2. [Rule 2 - Missing Critical] Fixed EditorViewportInteraction.cpp missing Trigger in 3 gizmo switches**
- **Found during:** Task 1 (build verification, compiler warnings)
- **Issue:** Three switches in viewport interaction had no Trigger case — triggers would be unselectable/unmovable via gizmos
- **Fix:** Added Trigger fallthrough to Mesh/Group group in all three switches (visibility check, single gizmo, multi gizmo)
- **Files modified:** `src/editor/viewport/EditorViewportInteraction.cpp`
- **Verification:** Build clean, no -Wswitch warnings remaining
- **Committed in:** `f42c4ac` (Task 1 commit)

**3. [Rule 2 - Missing Critical] Added Trigger inspector panel case in EditorInspectorPanel.cpp**
- **Found during:** Task 2 (build verification, compiler warning)
- **Issue:** Inspector switch had no Trigger case — selecting a trigger would show no properties
- **Fix:** Added Trigger inspector with Position, Shape combo, Half Extents/Radius DragFloat, and Fire Once checkbox
- **Files modified:** `src/editor/ui/EditorInspectorPanel.cpp`
- **Verification:** Build clean, no -Wswitch warnings remaining
- **Committed in:** `c34a2c8` (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (1 Rule 1 bug, 2 Rule 2 missing critical)
**Impact on plan:** All auto-fixes necessary for correctness. No scope creep — each fix addressed a compiler warning caused directly by adding the Trigger enum value.

## Issues Encountered
- The worktree build directory did not exist; required cmake configure step with Python venv for GLAD code generation (`-DPython_EXECUTABLE=...venv/bin/python3`)
- Build ran from worktree path (`/Users/ozan/Projects/gsd-3d-roguelike/.claude/worktrees/agent-a0d2a957/build/`) not main repo build directory

## Known Stubs
None — all trigger data flows through the real `TriggerPlacement` struct, serialized and deserialized by the existing `serializeLevelDef`/`loadLevelDef` code paths.

## Next Phase Readiness
- Foundation complete: Trigger objects are full scene citizens with CRUD, undo/redo, serialization, and outliner display
- 16-02 can now add the behavior authoring panel (list/add/remove BehaviorDeclaration entries on any selected Trigger)
- 16-03 can build the full behavior authoring UX on top of the completed foundation

---
*Phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity*
*Completed: 2026-04-04*
