---
phase: quick
plan: 260402-q3f
subsystem: editor/debug
tags: [debug-harness, input-simulation, imgui, imguizmo, diagnostics]
dependency_graph:
  requires: [260402-okw, 260402-prr]
  provides: [gizmo-freeze-diagnostics, input-simulation]
  affects: [level-editor]
tech_stack:
  added: []
  patterns: [ImGuiIO event injection, Unix socket debug commands]
key_files:
  created:
    - tools/diagnose-gizmo-freeze.py
  modified:
    - src/editor/debug/EditorCommander.h
    - src/editor/debug/EditorCommander.cpp
    - src/editor/debug/EditorInspector.h
    - src/editor/debug/EditorInspector.cpp
    - src/editor/debug/DebugHarness.h
    - src/editor/debug/DebugHarness.cpp
    - apps/level_editor/main.cpp
decisions:
  - Input injection uses ImGuiIO API (AddKeyEvent/AddMouseButtonEvent/AddMousePosEvent) directly, bypassing GLFW callbacks — correct approach since ImGui_ImplGlfw installs its own callback chain
  - drag() queues all events at once then returns immediately; events process across subsequent frames — use wait_frames() polling to observe state transitions
  - hold:true mode in drag() keeps mouse button pressed so diagnostic script can capture MID-drag state, then send a separate command.mouse_release
  - GLFWwindow* passed through DebugHarness constructor to EditorCommander so glfwPostEmptyEvent() can wake the editor event loop after injecting input
metrics:
  duration_minutes: 25
  completed_date: "2026-04-02"
  tasks_completed: 2
  files_changed: 7
---

# Quick Task 260402-q3f: Debug Harness Input Simulation and Gizmo Diagnostic Script Summary

Complete implementation of ImGuiIO-based input simulation commands and diagnostic state inspection in the editor debug harness, plus a Python diagnostic script that exercises gizmo and undo/redo operations to identify stuck states.

## What Was Built

### Input Simulation (EditorCommander)

Three previously-stubbed commands are now fully implemented using the ImGuiIO event queue API:

- `command.key_press` — injects key press+release with modifier support (ctrl, super, shift, alt). Maps key name strings (A-Z, F1-F12, Delete, Backspace, Escape, arrow keys, etc.) to ImGuiKey values via a lookup function. Calls `glfwPostEmptyEvent()` to wake the editor loop.
- `command.mouse_click` — injects mouse position + button press + release events.
- `command.drag` — injects a multi-step mouse drag. Supports `hold:true` mode that keeps the button pressed, allowing mid-drag state inspection before a separate `command.mouse_release` call.
- `command.mouse_release` — releases a held mouse button. New command, not previously planned.

All four commands are registered in DebugHarness. `GLFWwindow*` is now threaded through `DebugHarness` and `EditorCommander` constructors so `glfwPostEmptyEvent()` can be called.

### Diagnostic Inspect Commands (EditorInspector)

Three new read-only inspect commands:

- `inspect.imgui_capture` — reports `WantCaptureMouse`, `WantCaptureKeyboard`, `WantTextInput`, hovered window name, active ID and its window. Requires `imgui_internal.h`.
- `inspect.imguizmo_state` — reports `ImGuizmo::IsOver()`, `IsUsing()`, `IsUsingAny()`, `IsUsingViewManipulate()`, `IsViewManipulateHovered()`.
- `inspect.gizmo_detailed` — combined view: tool mode, all ImGuizmo state, mouse position and left-down state, selection count, undo/redo availability and labels.

### Diagnostic Script (tools/diagnose-gizmo-freeze.py)

A Python script that connects to the running editor via the debug harness socket and runs 6 test scenarios:

1. Translate drag — inject drag with hold, capture MID state, release, capture POST
2. Scale drag — same pattern for the gizmo mode most likely to freeze
3. Undo after gizmo — exercises undo and checks for stuck capture state and selection loss
4. Redo after undo — exercises redo with same checks
5. Rapid undo/redo cycle (3x) — stress test for undo stack corruption
6. Viewport-escape drag — drag to negative x coordinates to simulate mouse leaving the window

**Stuck-state detection logic:**
- `ImGuizmo::IsUsing=true AND mouse_left_down=false` → gizmo stuck in drag mode
- `WantCaptureMouse=true AND IsUsing=false AND IsOver=false` → orphaned ImGui mouse capture
- Selection count drops to 0 after undo when objects were selected → selection pruning issue

Accepts `--socket`, `--verbose`, and `--json` flags. Handles connection errors gracefully with clear messages. Auto-discovers editor socket via `/tmp/pixel-roguelike-editor-*.sock` glob.

## Commits

- `8b53d3d` — Implement input simulation and diagnostic inspect commands in debug harness
- `08b8502` — Add gizmo freeze diagnostic script

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None. All commands are fully implemented.

## Self-Check: PASSED

- `/Users/ozan/Projects/gsd-3d-roguelike/tools/diagnose-gizmo-freeze.py` — file exists and is executable
- `8b53d3d` — commit exists (Task 1)
- `08b8502` — commit exists (Task 2)
- `cmake --build build --target level-editor` — builds cleanly with no errors
- `python3 tools/diagnose-gizmo-freeze.py --help` — outputs correct usage with --socket, --verbose, --json flags
