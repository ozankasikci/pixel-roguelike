---
phase: 09-selection-overlay-depth-fix
plan: 01
subsystem: rendering
tags: [opengl, depth-test, wireframe, editor, selection]

# Dependency graph
requires:
  - phase: 08-create-institutional-room-scene-from-concept-art
    provides: Scene with multiple objects at varying depths to test selection
provides:
  - Depth-correct selection overlay with two-pass rendering (ghost + primary)
  - Professional-quality selection highlight that respects 3D occlusion
affects: [10-global-shortcuts-hover-highlight]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Two-pass overlay: ghost wireframe (ignoreDepth=true, 20% tint) drawn first, primary wireframe (ignoreDepth=false, full tint) drawn second"

key-files:
  created: []
  modified:
    - src/editor/render/EditorScenePreviewRenderer.cpp

key-decisions:
  - "Two-pass approach instead of stencil-based: simpler, leverages existing ignoreDepth flag in RenderObject"
  - "Ghost wireframe at 20% brightness with thinner lines (2.0/1.5) to differentiate from primary (4.0/2.5)"

patterns-established:
  - "Two-pass overlay pattern: push ghost pass (ignoreDepth=true, dim tint) then primary pass (ignoreDepth=false, full tint) for depth-aware highlights"

requirements-completed: [SEL-01]

# Metrics
duration: 5min
completed: 2026-04-01
---

# Phase 9 Plan 1: Selection Overlay Depth Fix Summary

**Two-pass depth-correct selection overlay -- ghost wireframe at 20% brightness for occluded objects, full-brightness depth-tested wireframe for visible portions**

## Performance

- **Duration:** 5 min
- **Started:** 2026-04-01
- **Completed:** 2026-04-01
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Selection wireframe no longer bleeds through walls and occluding geometry at full brightness
- Occluded objects show a faint 20% ghost outline confirming selection without obscuring foreground
- Primary/secondary selection colors (yellow/gold and cyan) preserved unchanged

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement two-pass depth-correct selection overlay** - `8d0a4ce` (fix)
2. **Task 2: Verify depth-correct selection overlay in editor** - checkpoint:human-verify (approved)

## Files Created/Modified
- `src/editor/render/EditorScenePreviewRenderer.cpp` - Modified `appendSelectionOverlays()` to push two RenderObjects per selection: ghost pass (ignoreDepth=true, 20% tint, thinner lines) then primary pass (ignoreDepth=false, full tint, original line widths)

## Decisions Made
- Used two-pass approach leveraging existing `ignoreDepth` flag rather than adding stencil buffer complexity
- Ghost wireframe at 20% tint multiplier provides visibility without visual noise
- Ghost lines thinner (2.0/1.5) than primary (4.0/2.5) for clear visual hierarchy

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Selection overlay fix complete, Phase 10 (Global Keyboard Shortcuts and Hover Highlight) can proceed
- Hover highlight in Phase 10 can reuse the two-pass pattern for consistent depth-aware feedback

## Self-Check: PASSED

- FOUND: src/editor/render/EditorScenePreviewRenderer.cpp
- FOUND: commit 8d0a4ce
- FOUND: 09-01-SUMMARY.md

---
*Phase: 09-selection-overlay-depth-fix*
*Completed: 2026-04-01*
