---
phase: 11-add-mesh-discoverability
plan: 01
subsystem: ui
tags: [imgui, level-editor, mesh-placement, editor-ux]

# Dependency graph
requires:
  - phase: 10-global-shortcuts-hover
    provides: EditorPlacementState, beginPlacement, commitPlacement, selectedIds wiring patterns

provides:
  - Add Mesh toolbar button with filtered popup picker (always visible, no shortcut needed)
  - commitPlacement returns std::optional<uint64_t> for auto-selection
  - Auto-select placed mesh (both click-to-place and drag-and-drop)

affects: [level-editor, editor-ux, mesh-placement]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - ImGui BeginPopup with BeginChild for scrollable filtered picker
    - std::transform tolower for case-insensitive substring filter
    - Return value from mutation functions enables immediate auto-selection

key-files:
  created: []
  modified:
    - src/editor/viewport/EditorViewportInteraction.h
    - src/editor/viewport/EditorViewportInteraction.cpp
    - apps/level_editor/main.cpp

key-decisions:
  - "commitPlacement returns std::optional<uint64_t>: result variable pattern with Mesh case setting it, all others leaving nullopt"
  - "Add Mesh button placed immediately after Add button with SameLine for toolbar cohesion"
  - "Filter cleared (addMeshFilter[0] = 0) each time popup opens so previous search does not persist"

patterns-established:
  - "Picker popup pattern: ImGui::BeginPopup + SetNextWindowSize + InputText filter + BeginChild list + Selectable items"
  - "Auto-select after placement: capture commitPlacement return value, check has_value(), set selectedIds + inspectorContext"

requirements-completed: [DISC-01]

# Metrics
duration: 2min
completed: 2026-04-01
---

# Phase 11 Plan 01: Add Mesh Discoverability Summary

**Discoverable 'Add Mesh' toolbar button with real-time filtered popup picker and auto-selection of placed meshes**

## Performance

- **Duration:** 2 min
- **Started:** 2026-04-01T15:37:17Z
- **Completed:** 2026-04-01T15:39:27Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Changed `commitPlacement` from `void` to `std::optional<std::uint64_t>` — Mesh placements return the new object ID, all others return `std::nullopt`
- Added "Add Mesh" button in viewport toolbar (always visible, no right-click or shortcut needed)
- Popup picker shows all MeshLibrary meshes with real-time case-insensitive substring filter; filter clears on each popup open
- Placed meshes are now auto-selected in viewport and inspector for both click-to-place and drag-and-drop paths

## Task Commits

Each task was committed atomically:

1. **Task 1: Extend commitPlacement to return the placed object ID** - `03146a9` (feat)
2. **Task 2: Add "Add Mesh" toolbar button with filtered popup picker and auto-select** - `b7bda84` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `src/editor/viewport/EditorViewportInteraction.h` - Changed `commitPlacement` declaration to return `std::optional<std::uint64_t>`
- `src/editor/viewport/EditorViewportInteraction.cpp` - Changed implementation: `result = document.addMesh(placement)` in Mesh case, `return result` at end
- `apps/level_editor/main.cpp` - Added `addMeshFilter` buffer, "Add Mesh" button, `AddMeshPicker` popup with filter+child list, auto-select in both placement call sites

## Decisions Made
- Used a local `std::optional<std::uint64_t> result` variable in `commitPlacement` to avoid multiple return points — cleaner than adding `return` to every case branch
- Filter variable declared at frame scope (not static) to avoid stale state between popup sessions — cleared explicitly on each open
- Both placement call sites updated for consistency: click-to-place and drag-and-drop both auto-select

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 11 complete. DISC-01 requirement fulfilled — meshes are now discoverable via always-visible toolbar button.
The level editor now matches the Unity/Unreal pattern: Add Mesh -> filter -> click to pick -> click to place -> auto-selected.
v1.1 Editor UX milestone can proceed to next planned phase.

---
*Phase: 11-add-mesh-discoverability*
*Completed: 2026-04-01*
