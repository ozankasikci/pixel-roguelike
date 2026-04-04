---
phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity
plan: 03
subsystem: editor
tags: [trigger, gizmo, resize-handle, viewport, interaction-ring, imgui, opengl]

requires:
  - phase: 16-01
    provides: Trigger as first-class EditorSceneObject, EditorSceneObjectKind::Trigger
  - phase: 16-02
    provides: Trigger inspector with shape/halfExtents/radius editing, interactable.has_value()

provides:
  - Trigger volume resize gizmo handles (6 yellow solid cubes at face centers when selected)
  - Interaction distance ring (warm amber wireframe at interactable.distance for mesh objects)
  - showTriggers toggle wired into Helpers popup and appendHelperObjects call
  - TriggerHandleAxis enum and TriggerHandleDragState for drag-to-resize
  - tryBeginTriggerHandleDrag / updateTriggerHandleDrag / endTriggerHandleDrag functions
  - Resize Trigger undo command pushed on mouse release

affects:
  - 16-04 (human verification checkpoint)
  - future phases that use trigger authoring

tech-stack:
  added: []
  patterns:
    - "Screen-space handle hit-testing: project world position to screen via viewProj, compare 8px radius to mouse pos"
    - "Ray-axis projection: dot(toCenter, axis) / dot(axis, rayDir) gives t for extent dragging"
    - "TriggerHandleDragState: captures before-state on click, pushes DocumentStateCommand on release"

key-files:
  created: []
  modified:
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/viewport/EditorViewportInteraction.h
    - src/editor/viewport/EditorViewportInteraction.cpp
    - apps/level_editor/main.cpp (worktree)

key-decisions:
  - "Screen-space hit-testing (not ImGuizmo) for trigger resize handles — handles are simple 8px hit radius, no need for ImGuizmo overhead"
  - "Render handle cubes as solid yellow (not wireframe) with ignoreDepth so they are always visible"
  - "Interaction distance ring always visible (not just when selected) per D-10 contract — designers need to see interaction radius at all times"
  - "Show Triggers checkbox placed in Helpers popup (same as Show Colliders) rather than a separate menu"

patterns-established:
  - "TriggerHandleDragState pattern: clear on Escape, tryBegin on click (before gizmo), update on drag, end on release"
  - "appendHelperObjects must receive ui.showTriggers explicitly — default argument in header provides backward compat"

requirements-completed: []

duration: 25min
completed: 2026-04-04
---

# Phase 16 Plan 03: Trigger Resize Gizmo and Interaction Ring Summary

**Trigger resize gizmo handles (6 yellow face cubes), warm amber interactable distance ring, and drag-to-resize with undo via screen-space hit-testing**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-04-04
- **Completed:** 2026-04-04
- **Tasks:** 2 (+ checkpoint Task 3 awaiting human verify)
- **Files modified:** 4

## Accomplishments

- Trigger resize handles: when a trigger is selected, 6 yellow solid cubes render at face centers (box: at halfExtents boundaries; sphere: at radius along cardinal axes)
- Interaction distance ring: all mesh objects with interactable.has_value() show a warm amber (#CC9933) wireframe cube at interaction distance, always visible
- Show Triggers checkbox added to Helpers popup in viewport toolbar — wired to appendHelperObjects
- TriggerHandleDragState: screen-space 8px hit-test on handle positions, ray-axis projection during drag, pushes "Resize Trigger" undo command on release

## Task Commits

1. **Task 1: Trigger resize handles and interaction distance ring** — `e32e437` (main repo) + `aaf7795` (worktree main.cpp showTriggers pass-through)
2. **Task 2: Show Triggers toggle and trigger handle drag wiring** — `0ace5db` (main repo: EditorViewportInteraction.h/.cpp) + `a62e23d` (worktree: main.cpp drag wiring + Show Triggers checkbox)

## Files Created/Modified

- `src/editor/render/EditorScenePreviewRenderer.cpp` — Added resize handles for selected triggers (6 yellow solid cubes), interaction distance ring (warm amber wireframe) for interactable meshes
- `src/editor/viewport/EditorViewportInteraction.h` — Added TriggerHandleAxis enum, TriggerHandleDragState struct, function declarations for begin/update/end drag
- `src/editor/viewport/EditorViewportInteraction.cpp` — Implemented tryBeginTriggerHandleDrag (screen-space hit-test), updateTriggerHandleDrag (ray projection), endTriggerHandleDrag (undo command push)
- `apps/level_editor/main.cpp` — Wired showTriggers to appendHelperObjects, added TriggerHandleDragState var, wired drag lifecycle (click/drag/release/escape), added Show Triggers checkbox to Helpers popup

## Decisions Made

- Screen-space hit-testing (not ImGuizmo) for trigger handles — 8px radius is sufficient; ImGuizmo would add unnecessary complexity for simple cube handles
- Handle cubes rendered as solid (not wireframe) with ignoreDepth — ensures they are always visible and visually distinct from wireframe volume
- Interaction distance ring always visible (not selection-gated) per D-10 — designers need persistent visibility of interaction radius
- Show Triggers placed in Helpers popup matching the existing showColliders/showLightHelpers/showSpawnMarker pattern

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] appendHelperObjects call site wasn't passing showTriggers**

- **Found during:** Task 1 (trigger wireframe guard review)
- **Issue:** The existing call in `main.cpp` passed only 3 show flags (`showColliders, showLightHelpers, showSpawnMarker`), missing `showTriggers`. The function signature had a default value of `true` masking this, meaning the toggle would never work.
- **Fix:** Added `ui.showTriggers` as the 4th boolean argument to `appendHelperObjects` in the main.cpp call site.
- **Files modified:** `apps/level_editor/main.cpp`
- **Verification:** Build succeeds; showTriggers now controls trigger wireframe visibility.
- **Committed in:** `aaf7795` (part of Task 1 worktree commit)

---

**Total deviations:** 1 auto-fixed (Rule 3 - blocking)
**Impact on plan:** Single missing argument; essential for showTriggers to actually work. No scope creep.

## Issues Encountered

None — implementation followed plan exactly with one auto-fix for a missing argument.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Trigger resize gizmo handles are rendered and functional for drag-to-resize
- Interaction distance ring renders for all interactable meshes
- Show Triggers toggle works end-to-end
- Round-trip test (`test_behavior_trigger_roundtrip`) passes: exit code 0
- Ready for Task 3: human verification of the full trigger/behavior authoring workflow

---
*Phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity*
*Completed: 2026-04-04*

## Self-Check: PASSED

- FOUND: src/editor/render/EditorScenePreviewRenderer.cpp
- FOUND: src/editor/viewport/EditorViewportInteraction.h
- FOUND: src/editor/viewport/EditorViewportInteraction.cpp
- FOUND: .planning/phases/16-.../16-03-SUMMARY.md
- FOUND: e32e437 feat(16-03): add trigger resize handles and interactable distance ring
- FOUND: 0ace5db feat(16-03): add trigger handle drag-to-resize with undo support
- FOUND: aaf7795 feat(16-03): pass showTriggers to appendHelperObjects (worktree)
- FOUND: a62e23d feat(16-03): wire trigger handle drag and Show Triggers toggle (worktree)
