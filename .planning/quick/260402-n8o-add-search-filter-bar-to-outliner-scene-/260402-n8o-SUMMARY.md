# Quick Task 260402-n8o: Add search filter bar to outliner — Summary

**Completed:** 2026-04-02
**Status:** Done

## What Changed

Added a search/filter input at the top of the Outliner (scene hierarchy) panel:

- Text input with "Search..." placeholder hint, full width
- Case-insensitive substring matching against object labels
- Matching objects AND their ancestors are shown to preserve hierarchy context
- Tree nodes auto-expand when a filter is active so matches aren't hidden
- Empty filter shows all objects (no change from previous behavior)

## Files Modified

| File | Change |
|------|--------|
| `src/editor/ui/LevelEditorUi.h` | Added `outlinerFilter[128]` char buffer to `EditorUiState` |
| `src/editor/ui/EditorOutlinerPanel.cpp` | Search input + filter logic with ancestor visibility |
