---
phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity
plan: 02
subsystem: ui
tags: [imgui, editor, inspector, behavior, trigger, interactable, undo-redo]

requires:
  - phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity plan 01
    provides: EditorSceneObjectKind::Trigger, TriggerPlacement in variant, addTrigger(), Trigger in outliner

provides:
  - Trigger inspector section with Shape/Position/Half Extents/Radius/Fire Once/Node ID property rows
  - Make Interactable checkbox on mesh entities with Prompt/Distance/Dot Threshold fields
  - Collapsible behavior event sections (On Activate/Enter/Exit/Timer) for Mesh, Light, and Trigger entities
  - Categorized two-combo action type dropdown (Door/Lighting/Audio/Entity/Timing)
  - Target node ID combo listing all scene entities with self first and type-to-filter
  - Action parameter widgets for SetLight (intensity/color/radius), PlaySound (sound ID), Delay (duration)
  - Add Action / Remove Action buttons with full undo/redo via EditorCommandStack

affects:
  - 16-03 (gizmo work for trigger volume visualization in viewport)
  - future behavior serialization/deserialization plan

tech-stack:
  added: []
  patterns:
    - "Behavior authoring: renderBehaviorSections + renderActionEntryRow helpers in anonymous namespace"
    - "coreActionCategories: static vector of category/action pairs for categorized action dropdown"
    - "endInspectorPropertyTable early exit pattern: end table inside switch case, then render extra sections, return early"

key-files:
  created: []
  modified:
    - src/editor/ui/EditorInspectorPanel.cpp

key-decisions:
  - "End inspector property table early inside Mesh/Light/Trigger cases to allow CollapsingHeader sections below the table"
  - "Use static char targetFilter[64] inside the combo popup for per-frame filter state (acceptable for single inspector)"
  - "Action categories limited to core subset only (Door/Lighting/Audio/Entity/Timing) per D-08 — no ShowMessage, FlickerLight, etc."
  - "Mesh interactable section uses standalone Checkbox outside the property table, then opens a new table for sub-fields"

requirements-completed: []

duration: 8min
completed: 2026-04-04
---

# Phase 16 Plan 02: Inspector UI for Trigger Properties, Behavior Editing, and Interactable Authoring

**ImGui inspector panels for trigger shape/size, behavior event sections with categorized action dropdowns, and Make Interactable toggle on mesh entities — all with full undo/redo via EditorCommandStack**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-04-04T19:29:00Z
- **Completed:** 2026-04-04T19:37:08Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- Trigger inspector section uses proper `renderInspectorPropertyRow` pattern with Shape dropdown, Position, Half Extents/Radius, Fire Once checkbox, and Node ID text field
- `renderBehaviorSections` function renders four collapsible event sections (On Activate/Enter/Exit/Timer) for Mesh, Light, and Trigger entities
- `renderActionEntryRow` renders per-action row with category combo, type combo, target node combo with "self" first and type-to-filter input, parameter widgets, and X remove button
- "Make Interactable" checkbox on mesh entities toggles Prompt/Distance/Dot Threshold sub-fields inline
- All mutations go through `captureState()`/`pushDocumentStateCommand()` for undo/redo

## Task Commits

1. **Task 1 + Task 2: Add trigger inspector, interactable, and behavior authoring sections** - `b795a0b` (feat)

**Plan metadata:** (pending final docs commit)

## Files Created/Modified

- `/Users/ozan/Projects/gsd-3d-roguelike/src/editor/ui/EditorInspectorPanel.cpp` - Added trigger property rows, interactable section, action category helpers, renderActionEntryRow, renderBehaviorSections, wired to Mesh/Light/Trigger cases

## Decisions Made

- **End table early pattern:** Mesh, Light, and Trigger cases now call `endInspectorPropertyTable()` inside the case and `return` early, allowing `CollapsingHeader` behavior sections below. Other cases fall through to the outer `endInspectorPropertyTable()`.
- **Static filter buffer:** The target node ID combo uses a `static char targetFilter[64]` inside the combo popup. This persists the filter text within a single frame without requiring per-object state — acceptable since only one inspector row is active at a time.
- **Core action subset:** Only Door, Lighting, Audio, Entity, and Timing categories are exposed per D-08. ShowMessage, FlickerLight, EmitEvent, LockPlayer, TeleportPlayer are not shown to keep the UI focused on the most common actions.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Replaced raw ImGui calls in existing Trigger case with renderInspectorPropertyRow**

- **Found during:** Task 1 (Trigger inspector section)
- **Issue:** Plan 01 had implemented a basic Trigger case using raw `ImGui::Combo`, `ImGui::DragFloat`, `ImGui::Checkbox` calls without the `renderInspectorPropertyRow` table row pattern. These raw calls rendered correctly in the existing table but would appear misaligned relative to other inspector fields. The plan specified using `renderInspectorPropertyRow`.
- **Fix:** Replaced all raw ImGui calls in the Trigger case with proper `renderInspectorPropertyRow` wrappers.
- **Files modified:** src/editor/ui/EditorInspectorPanel.cpp
- **Verification:** Build succeeds, Trigger case uses the table pattern.
- **Committed in:** b795a0b

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug fix / pattern correction)
**Impact on plan:** Required to satisfy acceptance criteria. No scope creep.

## Issues Encountered

None — build succeeded on first attempt.

## Known Stubs

None — behavior data is live from `TriggerPlacement::behaviors`, `LevelMeshPlacement::behaviors`, and `LevelLightPlacement::behaviors`. Interactable data is live from `LevelMeshPlacement::interactable`. No hardcoded placeholders.

## Next Phase Readiness

- Plan 02 complete: inspector UI for trigger properties, behavior authoring, and interactable editing
- Plan 03 (gizmo work) can now start — it needs the Trigger case in inspector to be functional (done)
- Behavior save/load round-trip depends on serialization plan (verify Plan 03 or 04 covers `.scene` file format)

## Self-Check: PASSED

- `src/editor/ui/EditorInspectorPanel.cpp` exists and was modified: FOUND
- Commit `b795a0b` exists: confirmed via `git rev-parse --short HEAD`
- Build `level-editor` succeeds: confirmed
- `renderBehaviorSections` called 3 times (mesh, light, trigger) + 1 definition: confirmed
- `coreActionCategories` with 5 entries: confirmed
- All acceptance criteria for Task 1 and Task 2: confirmed

---
*Phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity*
*Completed: 2026-04-04*
