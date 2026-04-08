---
phase: quick
plan: 260408-xaz
subsystem: editor/ui
tags: [outliner, viewport-sync, expansion-state, bug-fix]
dependency_graph:
  requires: []
  provides: [persistent-ancestor-expansion-on-scroll-to-selection]
  affects: [EditorOutlinerPanel]
tech_stack:
  added: []
  patterns: []
key_files:
  modified:
    - src/editor/ui/EditorOutlinerPanel.cpp
decisions:
  - "Kept expandForScroll set intact — it drives shouldOpen for the current frame before expandedOutlinerIds takes effect; only added a parallel insert into the persistent set"
metrics:
  duration: "3m"
  completed: "2026-04-08"
  tasks_completed: 1
  files_modified: 1
---

# Quick 260408-xaz: Fix Viewport-to-Hierarchy Sync — Persist Ancestor Expansion

**One-liner:** Added `ui.expandedOutlinerIds.insert(ancestorId)` inside the scroll-to-selection ancestor-walk loop so expanded ancestors persist across frames after a viewport click.

## What Was Done

**Task 1: Persist ancestor expansion in expandedOutlinerIds during scroll-to-selection**

Inside the `if (ui.scrollToSelection)` block in `EditorOutlinerPanel.cpp` (line ~316), the ancestor-walk loop was inserting ancestors only into the temporary per-frame `expandForScroll` set. On the next frame, `scrollToSelection` is cleared, `expandForScroll` is empty, and those ancestors collapsed.

Fix: added one line to also insert each ancestor into `ui.expandedOutlinerIds` (the persistent set). The `expandForScroll` set is retained because `shouldOpen` is evaluated during tree rendering on the same frame the flag is set — `expandedOutlinerIds` alone would not be sufficient for the initial frame.

- **Commit:** `08d7b7d`
- **File:** `src/editor/ui/EditorOutlinerPanel.cpp`

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Threat Flags

None.

## Self-Check: PASSED

- `src/editor/ui/EditorOutlinerPanel.cpp` — modified (confirmed)
- Commit `08d7b7d` — exists (confirmed via `git rev-parse --short HEAD`)
- Build: `level-editor` compiled cleanly with no errors or warnings related to EditorOutlinerPanel
