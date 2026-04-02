---
phase: quick
plan: 260402-rkd
subsystem: editor/debug
tags: [debug-harness, entity-listing, gizmo, camera, testing]
dependency_graph:
  requires: [260402-q3f]
  provides: [inspect.entities, inspect.world_to_screen, inspect.camera, inspect.gizmo_screen_pos, command.focus_entity, command.gizmo_drag, command.wait_events]
  affects: [apps/level_editor/main.cpp, src/editor/debug, external/ImGuizmo]
tech_stack:
  added: []
  patterns: [ImGuiContext.InputEventsQueue for pending event count, ImGuizmo::GetScreenCenter for gizmo position]
key_files:
  created: []
  modified:
    - src/editor/debug/DebugHarness.h
    - src/editor/debug/DebugHarness.cpp
    - src/editor/debug/EditorInspector.h
    - src/editor/debug/EditorInspector.cpp
    - src/editor/debug/EditorCommander.h
    - src/editor/debug/EditorCommander.cpp
    - apps/level_editor/main.cpp
    - external/ImGuizmo/ImGuizmo.h
    - external/ImGuizmo/ImGuizmo.cpp
decisions:
  - ImGuizmo::GetScreenCenter() added to external/ImGuizmo (not in bundled version); returns mScreenSquareCenter from gContext, valid after Manipulate() called
  - EditorViewportState viewportState moved from inside renderFrame lambda to before DebugHarness construction so harness can hold a const reference
  - pendingEventCount() uses ImGuiContext::InputEventsQueue.Size to report queued input events
  - inspect.gizmo_screen_pos returns raw ImGuizmo screen center with no window position subtraction (ViewportsEnable OFF means ImGui coords == GLFW cursor coords)
metrics:
  duration: ~30min
  completed_date: 2026-04-02
  tasks: 2
  files: 9
---

# Phase quick Plan 260402-rkd: Expand Debug Harness with Entity Listing Summary

Entity listing, camera focus, gizmo-aware drag, and wait-for-idle commands added to the debug harness; coordinate system normalized to GLFW cursor space with no window subtraction.

## What Was Built

### Task 1: Camera/viewport refs + entity listing + world-to-screen

**DebugHarness** now accepts `EditorCamera&`, `EditorCameraAnimation&`, `const EditorViewportState&`, and `const EditorPreviewWorld&` as constructor parameters. These are forwarded to both `EditorInspector` and `EditorCommander`.

In `main.cpp`, `EditorViewportState viewportState` was moved from inside the `renderFrame` lambda to before the `DebugHarness` construction, so the harness can safely hold a reference. The declaration site is reset to `{}` at the start of each frame.

**New inspect commands:**
- `inspect.entities` — returns all scene objects with `id`, `label`, `kind`, `world_position` (using `editorSceneObjectAnchor`)
- `inspect.world_to_screen` — given `{x, y, z}` world coordinates, projects to GLFW cursor space using current camera view/projection and viewport rect; returns `{x, y, visible}`
- `inspect.camera` — returns current camera state: position, yaw, pitch, fov, orbit_distance, orbit_pivot, orbit_pivot_valid
- `inspect.gizmo_screen_pos` — returns `ImGuizmo::GetScreenCenter()` directly in GLFW cursor space; no window position subtraction (since ViewportsEnable is OFF, ImGui screen coords == GLFW cursor coords)

**ImGuizmo extension:** Added `ImGuizmo::GetScreenCenter()` to both `ImGuizmo.h` and `ImGuizmo.cpp` since the bundled version did not include this accessor. It returns `gContext.mScreenSquareCenter` which is set during `Manipulate()`.

### Task 2: focus_entity, gizmo_drag, wait_events

**New command commands:**
- `command.focus_entity` — finds entity by label, looks up bounds in `EditorPreviewWorld`, calls `beginFocusAnimation()`. Falls back to anchor ± 0.5m unit cube if no mesh bounds found.
- `command.gizmo_drag` — reads current ImGuizmo screen center, validates it is within window bounds (returning descriptive error if not), normalizes direction vector, then delegates to `drag()`. Supports `direction` as string ("right", "left", "up", "down") or object `{dx, dy}`. Default: right, 50px, 10 steps.
- `command.wait_events` — returns `{pending_events, camera_animating, idle}` for polling. `pending_events` reads `ImGuiContext::InputEventsQueue.Size`; `idle` is true when both are zero/false.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking Issue] ImGuizmo::GetScreenCenter() not in bundled version**
- **Found during:** Task 1 compilation
- **Issue:** The plan's context referenced `ImGuizmo::GetScreenCenter()` as if it existed, but the project's bundled ImGuizmo (external/ImGuizmo/) does not expose this function.
- **Fix:** Added `GetScreenCenter()` declaration to `ImGuizmo.h` and implementation to `ImGuizmo.cpp`. Returns `gContext.mScreenSquareCenter` which is already set by the existing Manipulate() code.
- **Files modified:** `external/ImGuizmo/ImGuizmo.h`, `external/ImGuizmo/ImGuizmo.cpp`
- **Commit:** 60fa93d

## Self-Check: PASSED

- `src/editor/debug/EditorInspector.h` - FOUND
- `src/editor/debug/EditorInspector.cpp` - FOUND
- `src/editor/debug/EditorCommander.h` - FOUND
- `src/editor/debug/EditorCommander.cpp` - FOUND
- `src/editor/debug/DebugHarness.h` - FOUND
- `src/editor/debug/DebugHarness.cpp` - FOUND
- `external/ImGuizmo/ImGuizmo.h` - FOUND
- `external/ImGuizmo/ImGuizmo.cpp` - FOUND
- `apps/level_editor/main.cpp` - FOUND
- Commit 60fa93d - FOUND
- Commit edbfff1 - FOUND
- Build: level-editor compiles cleanly (zero errors)
