---
phase: 10-global-keyboard-shortcuts-and-hover-highlight
plan: 02
subsystem: editor
tags: [opengl, wireframe, raycast, hover, selection, editor-viewport]

# Dependency graph
requires:
  - phase: 10-global-keyboard-shortcuts-and-hover-highlight
    provides: Phase 10-01 keyboard shortcuts (Delete, Escape, Ctrl+D, F) from which hover suppression reuses the same input guards
  - phase: 09-selection-depth-fix
    provides: Two-pass selection overlay pattern (appendSelectionOverlays) that appendHoverOverlay follows

provides:
  - appendHoverOverlay function in EditorScenePreviewRenderer showing blue-white depth-tested wireframe on hover
  - Per-frame hover raycast in main.cpp using pickEditorObject + buildEditorRay
  - Hover suppression for camera manipulation (RMB orbit, alt+LMB, MMB pan), gizmo drag, placement mode, and play preview
  - Removal of the selection picker popup overlay (replaced by hover highlight)

affects:
  - future editor UX phases that modify viewport interaction
  - any phase touching EditorScenePreviewRenderer.cpp overlay pass ordering

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "appendHoverOverlay follows same RenderObject push pattern as appendSelectionOverlays: get cube mesh, findObjectBounds, pad size 1.035x, push wireframe RenderObject"
    - "Hover suppression: single bool suppressHover computed from 6 conditions before raycast — avoids redundant pickEditorObject calls when cursor interaction is irrelevant"
    - "ignoreDepth=false on hover wireframe ensures depth-tested single-pass (no ghost through occluding geometry)"

key-files:
  created: []
  modified:
    - src/editor/render/EditorScenePreviewRenderer.h
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - apps/level_editor/main.cpp

key-decisions:
  - "Hover color (0.55, 0.85, 1.00) produces cool blue-white visually distinct from selection gold and secondary cyan without a separate alpha channel"
  - "appendHoverOverlay internally guards against selected objects — callers pass hoveredId=0 for no-hover, function skips if id is in selectedIds"
  - "Selection picker popup overlay removed by orchestrator during verification — hover highlight provides the same pre-click affordance and the popup was redundant"

patterns-established:
  - "Hover overlay pattern: depth-tested wireframe (ignoreDepth=false), single pass, lineWidth 2.0, cool blue-white tint"
  - "Suppression pattern: compute suppressHover bool once before raycast; skip pickEditorObject entirely when suppressed"

requirements-completed: [SEL-02]

# Metrics
duration: 30min
completed: 2026-04-01
---

# Phase 10 Plan 02: Hover Highlight Summary

**Per-frame hover raycast with depth-tested blue-white wireframe on unselected viewport objects, suppressed during all camera/gizmo/placement interactions, replacing the selection picker popup**

## Performance

- **Duration:** ~30 min
- **Started:** 2026-04-01T13:10:00Z
- **Completed:** 2026-04-01T17:42:22Z
- **Tasks:** 2 (1 implementation + 1 visual verification checkpoint)
- **Files modified:** 3

## Accomplishments

- Added `appendHoverOverlay` to `EditorScenePreviewRenderer` — depth-tested blue-white wireframe bounding box, lineWidth 2.0, skips already-selected objects
- Wired per-frame hover raycast in `main.cpp` using `pickEditorObject` + `buildEditorRay`; hover suppressed during RMB orbit, alt+LMB orbit, MMB pan, gizmo drag, placement mode, and play preview
- All 18 visual verification steps confirmed passing by user — hover, delete guard, escape, duplicate offset, and camera framing all working correctly
- Selection picker popup overlay removed (bonus deviation during verification) — hover highlight provides equivalent pre-click affordance without the intrusive UI

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement appendHoverOverlay and wire per-frame hover raycast** - `6d2d3c9` (feat)
2. **Bonus: Remove selection picker overlay (orchestrator change during verification)** - `41a76f2` (refactor)

## Files Created/Modified

- `src/editor/render/EditorScenePreviewRenderer.h` - Added `appendHoverOverlay` declaration
- `src/editor/render/EditorScenePreviewRenderer.cpp` - Implemented `appendHoverOverlay` (38 lines added)
- `apps/level_editor/main.cpp` - Per-frame hover raycast + suppression logic + selection picker popup removal

## Decisions Made

- Hover color `(0.55, 0.85, 1.00)` chosen for visual distinctness from selection gold `(1.30, 0.92, 0.24)` and secondary cyan `(0.50, 1.00, 0.62)`; the lower-than-1.0 RGB values produce a visually softer feel without a separate alpha channel
- `appendHoverOverlay` self-guards: if `hoveredId` is in `selectedIds`, early-returns — callers do not need to check; this keeps the caller contract simple
- `ignoreDepth=false` on hover RenderObject: single depth-tested pass only, no ghost wireframe through occluding geometry (matches plan requirement D-02)
- Suppression computed as a single `suppressHover` bool before the raycast, so `pickEditorObject` is never called when the cursor is in a manipulated state

## Deviations from Plan

### Auto-fixed / Bonus Changes

**1. [Bonus - Orchestrator] Removed selection picker popup overlay**
- **Found during:** Task 2 (visual verification)
- **Issue:** The "Pick object / Click again to cycle" popup was redundant now that hover highlight provides pre-click visual feedback; user chose to remove it during verification
- **Fix:** Removed 3 lines of ImGui popup rendering code from `main.cpp` (click-to-select and shift-click multi-select state machine unchanged)
- **Files modified:** `apps/level_editor/main.cpp`
- **Committed in:** `41a76f2` (separate commit by orchestrator, not part of task commit)

---

**Total deviations:** 1 bonus change (orchestrator-driven during verification)
**Impact on plan:** Scope-narrowing improvement — hover highlight makes the popup obsolete. No regressions.

## Issues Encountered

None — implementation followed the plan pattern from `appendSelectionOverlays` exactly. Build was clean on first attempt.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None — hover raycast, overlay rendering, and all suppression conditions are fully wired.

## Next Phase Readiness

- Phase 10 is fully complete: selection depth fix (09), global keyboard shortcuts + animated framing (10-01), and hover highlight (10-02) all verified
- Editor UX milestone v1.1 features are done; next phase (11-add-mesh-discoverability) can proceed
- No blockers

---
*Phase: 10-global-keyboard-shortcuts-and-hover-highlight*
*Completed: 2026-04-01*
