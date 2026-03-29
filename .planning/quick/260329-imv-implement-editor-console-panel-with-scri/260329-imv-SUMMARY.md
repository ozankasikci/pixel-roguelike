---
type: quick
id: 260329-imv
description: "Editor Console panel with spdlog ring-buffer sink, severity filtering, color coding, and dock integration"
completed: "2026-03-29T10:34:58Z"
tasks_completed: 2
tasks_total: 2
commits:
  - hash: cf61b85
    message: "Add EditorConsoleSink ring-buffer spdlog sink and ConsoleLogStore"
  - hash: 09c34ae
    message: "Wire Console panel into editor: sink, rendering, dock layout, and Window menu"
files_created:
  - src/engine/core/EditorConsoleSink.h
files_modified:
  - apps/level_editor/main.cpp
  - src/editor/ui/LevelEditorUi.h
  - src/editor/core/LevelEditorCore.h
  - src/editor/core/LevelEditorCore.cpp
  - src/editor/core/EditorLayoutPreset.h
  - src/editor/core/EditorLayoutPreset.cpp
---

# Quick Task 260329-imv: Editor Console Panel Summary

**One-liner:** Console panel capturing all spdlog output via an `EditorConsoleSink<std::mutex>` ring-buffer sink (2000-entry cap), docked bottom-tab alongside Asset Browser and Build Output, with Info/Warn/Error toggles and color-coded rendering.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Create ring-buffer spdlog sink and Console log store | cf61b85 | src/engine/core/EditorConsoleSink.h |
| 2 | Wire Console sink, panel rendering, dock layout, and Window menu | 09c34ae | apps/level_editor/main.cpp + 5 editor files |

## What Was Built

### Task 1 — EditorConsoleSink.h
New header `src/engine/core/EditorConsoleSink.h` provides:
- `ConsoleLogEntry` struct with `message`, `level`, and `timestamp` fields
- `ConsoleLogStore` — thread-safe ring buffer (mutex-protected, 2000-entry default, wraps on overflow)
- `EditorConsoleSink<Mutex>` — spdlog `base_sink` that formats log messages and pushes them to the store

### Task 2 — Full Editor Integration
- `EditorConsoleSink<std::mutex>` registered on the default spdlog logger at `main()` startup, alongside the existing stdout sink
- `showConsole = false` added to `EditorUiState` and `EditorLayoutVisibility`
- `show_console` key added to layout preset load/save in `EditorLayoutPreset.cpp`
- `buildDefaultEditorDockLayout()` extended with `consoleWindowName` parameter, docked to `bottomId`
- `Window > Panels > Console` menu item wired to `ui.showConsole`
- Console panel rendered with:
  - Info/Warn/Error filter toggle buttons with per-severity entry counts
  - Clear button that calls `consoleLogStore->clear()`
  - `ImGuiListClipper` for efficient virtual scrolling
  - Red text (`1.0f, 0.3f, 0.3f`) for errors, yellow (`1.0f, 0.8f, 0.0f`) for warnings, default for info/debug
  - Auto-scroll: advances to bottom when scroll position is at maximum

## Deviations from Plan

**1. [Rule 2 - Missing functionality] Added `show_build_output` to layout preset load/save**
- Found during: Task 2
- Issue: `show_build_output` was written in `saveEditorLayoutPreset` only as `show_viewport`, and the load path had no handler for `show_build_output` — the field was never persisted across preset saves/loads.
- Fix: Added `show_build_output` to both `loadEditorLayoutPreset` and `saveEditorLayoutPreset` in `EditorLayoutPreset.cpp`.
- Files modified: `src/editor/core/EditorLayoutPreset.cpp`
- Commit: 09c34ae

## Known Stubs

None — all data flows are wired. The `consoleLogStore` shared_ptr is live at `main()` scope throughout the editor session.

## Self-Check: PASSED

- `src/engine/core/EditorConsoleSink.h` — FOUND
- Commit `cf61b85` — FOUND (`git log` confirmed)
- Commit `09c34ae` — FOUND (`git log` confirmed)
- `cmake --build build --target level-editor` — exits 0 with no errors
