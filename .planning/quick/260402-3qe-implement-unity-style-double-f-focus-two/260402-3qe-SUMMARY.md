# Quick Task 260402-3qe: Unity-style double-F focus — Summary

**Completed:** 2026-04-02
**Status:** Done

## What Changed

Added Unity-style two-stage focus (F key) to the level editor:

1. **First F press:** Frames the full bounding box of all selected objects (existing behavior, unchanged)
2. **Second F press:** Zooms in tight to a 1-unit cube centered on the selection's center — camera positions at minimum distance (~3.0 units)
3. **Third F press:** Returns to full bounds view (alternates indefinitely)
4. **Selection change:** Resets toggle so a new selection always starts with full-bounds framing

## Files Modified

| File | Change |
|------|--------|
| `src/editor/viewport/EditorViewportController.h` | Added `FocusToggleState` struct (wasFocused bool + lastFocusedSelection vector) |
| `apps/level_editor/main.cpp` | Added focusToggle state variable; replaced single-mode focus block with toggle logic |

## Implementation Details

- `FocusToggleState.wasFocused` flips on each F press, selecting between full-bounds and tight-zoom modes
- `FocusToggleState.lastFocusedSelection` stores the selection vector from the last focus; when it differs from current `selectedIds`, the toggle resets to false
- Tight zoom passes a 1-unit cube (`center +/- 0.5`) to the existing `beginFocusAnimation`, which uses `focusEditorCameraOnBounds` with `max(radius * 2.4, 3.0)` distance formula — radius ~0.87 for the 1-unit cube means camera at 3.0 units
- Smooth animation via existing `EditorCameraAnimation` ease-out cubic preserved for both modes
