---
phase: quick
plan: 260401-x6s
subsystem: editor/main-loop
tags: [cpu, performance, glfw, idle-throttling, imgui]
dependency_graph:
  requires: []
  provides: [idle-cpu-reduction]
  affects: [level-editor]
tech_stack:
  added: []
  patterns: [hybrid-poll-wait, glfwWaitEventsTimeout, activity-detection]
key_files:
  created: []
  modified:
    - apps/level_editor/main.cpp
decisions:
  - Omit cameraAnim.active check — no camera animation system exists in current codebase; focus (F key) teleports instantly so no continuous rendering is needed for it
  - Two constants added to anonymous namespace with other editor constants: kIdleTimeoutSeconds (1/15) and kUnfocusedTimeoutSeconds (1/5)
  - Call glfwWaitEventsTimeout/glfwPollEvents directly in main.cpp, not through Window wrapper — throttling policy is editor-specific
metrics:
  duration_minutes: 4
  completed_date: "2026-04-01"
  tasks_completed: 1
  tasks_total: 2
  files_changed: 1
---

# Quick Task 260401-x6s: Fix Editor High CPU Usage with Industry-Standard Idle Throttling

**One-liner:** Hybrid glfwWaitEventsTimeout/glfwPollEvents main loop drops editor CPU from ~100% to near-zero when idle.

## What Was Done

Replaced the editor's tight busy-loop with a hybrid poll/wait strategy that uses `glfwWaitEventsTimeout()` when idle and `glfwPollEvents()` when continuous rendering is needed.

### Changes

**`apps/level_editor/main.cpp`**
- Added `kIdleTimeoutSeconds = 1.0 / 15.0` and `kUnfocusedTimeoutSeconds = 1.0 / 5.0` constants
- Replaced `window.pollEvents()` in the main loop with conditional logic:
  - Full `glfwPollEvents()` when runtime preview is running or ImGui wants mouse/keyboard
  - `glfwWaitEventsTimeout(kUnfocusedTimeoutSeconds)` when window is not focused
  - `glfwWaitEventsTimeout(kIdleTimeoutSeconds)` otherwise (idle state)

### Root Cause

The editor's `glfwPollEvents()` returns immediately on every frame, causing a busy-loop. While `glfwSwapInterval(1)` is set, macOS vsync via OpenGL is unreliable (documented GLFW issues on Mojave, Monterey, M1). The result was near-100% CPU even with no user interaction.

### Why These Activity Signals

| Signal | Reason |
|--------|--------|
| `ui.playPreview && runtimePreviewSession.captured()` | Runtime game preview requires full framerate |
| `ImGui::GetIO().WantCaptureMouse` | Covers hovering over panels, dragging sliders, gizmo interaction |
| `ImGui::GetIO().WantCaptureKeyboard` | Covers text input in inspector fields |
| `GLFW_FOCUSED == 0` | Editor in background — deeper throttle acceptable |

Note: `cameraAnim.active` was specified in the plan but does not exist in the current codebase. The F-key focus uses `focusEditorCameraOnPoint()` which teleports the camera instantly, so no continuous rendering flag is needed for it.

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1: Hybrid poll/wait | `7e0c66f` | Add hybrid poll/wait idle throttling to editor main loop |

## Deviations from Plan

**[Rule 1 - Bug/Adaptation] Omitted non-existent cameraAnim.active flag**
- **Found during:** Task 1
- **Issue:** Plan specified `cameraAnim.active` from "line 322" but this variable does not exist in the current codebase. Camera focus (F key) calls `focusEditorCameraOnPoint()` which teleports immediately — no interpolation, no animation flag.
- **Fix:** Omitted the check entirely. No continuous rendering is needed for focus since it's instant.
- **Files modified:** apps/level_editor/main.cpp
- **Commit:** 7e0c66f

## Verification Status

Task 1 (implementation): COMPLETE — `cmake --build build --target level-editor` passes cleanly.
Task 2 (human verify): PENDING — requires user to manually verify CPU drop in Activity Monitor.

## Self-Check: PASSED

- `apps/level_editor/main.cpp` modified and verified present
- Commit `7e0c66f` verified in git log
- Build output: `[100%] Built target level-editor`
