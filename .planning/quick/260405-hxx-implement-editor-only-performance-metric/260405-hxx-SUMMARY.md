---
phase: quick
plan: 260405-hxx
subsystem: editor/performance
tags: [editor, profiling, imgui, performance, timing]
dependency_graph:
  requires: []
  provides: [editor-performance-panel, per-subsystem-tick-timing]
  affects: [RuntimeGameSession, EditorUiState, level-editor]
tech_stack:
  added: []
  patterns: [chrono steady_clock instrumentation, ImGui table bar chart, editor-only panel pattern]
key_files:
  created:
    - src/editor/ui/EditorPerformancePanel.h
    - src/editor/ui/EditorPerformancePanel.cpp
  modified:
    - src/game/runtime/RuntimeGameSession.h
    - src/game/runtime/RuntimeGameSession.cpp
    - src/editor/ui/LevelEditorUi.h
    - src/editor/CMakeLists.txt
    - apps/level_editor/main.cpp
decisions:
  - "Performance panel placed in editor target only — zero impact on pixel-roguelike or gameplay builds"
  - "Menu item placed in View > Helpers submenu alongside Viewport Stats for logical grouping"
  - "Bar chart auto-scales: clamps to 2ms minimum range, expands if any value exceeds 2ms"
  - "Inventory timing: uses its own Clock::now() start since it follows a conditional; all other subsystems chain t0=t1 to avoid redundant clock reads"
metrics:
  duration: "~12 minutes"
  completed: "2026-04-05"
  tasks_completed: 2
  tasks_total: 2
  files_changed: 7
---

# Quick Task 260405-hxx: Editor-Only Performance Metrics Panel

Editor-only ImGui Performance panel showing per-subsystem tick timings (interaction/checkpoints/physics/inventory/movement/camera) and render pipeline timings (shadow/scene/SSAO/bloom/composite) with bar chart visualization, toggleable from View > Helpers > Performance.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Extend RuntimeSessionPerformanceStats with per-subsystem timings | 43def06 | RuntimeGameSession.h, RuntimeGameSession.cpp |
| 2 | Create EditorPerformancePanel and wire into editor UI | 0af2746 | EditorPerformancePanel.h, EditorPerformancePanel.cpp, LevelEditorUi.h, CMakeLists.txt, main.cpp |

## What Was Built

### Task 1 — RuntimeSessionPerformanceStats instrumentation

Added 7 new fields to `RuntimeSessionPerformanceStats`:
- `interactionMs`, `checkpointsMs`, `physicsMs`, `inventoryMs`, `movementMs`, `cameraMs`, `totalTickMs`

Instrumented `RuntimeGameSession::tick()` with `std::chrono::steady_clock` timing around each subsystem call. Overhead is negligible (~6 chrono::now() calls per frame, ~50ns each). Execution order is preserved exactly.

### Task 2 — EditorPerformancePanel

Created `EditorPerformancePanel.h/cpp` implementing `renderPerformancePanel()`:

- **Not in preview mode:** Shows centered disabled text "Enter Play Preview to see system timings"
- **In preview mode:**
  - **Gameplay Systems section:** 6-row table (Interaction, Checkpoints, Physics, Inventory, Movement, Camera) with ms value and amber bar chart. Auto-scales to 2ms minimum range.
  - **Render Pipeline section:** 5-row table (Shadow, Scene, SSAO, Bloom, Composite) plus draw/object/light/culled counts.
  - **Frame Summary section:** FPS, frame time, and breakdown: Tick + Render + Other = Total ms.

Panel is wired into:
- `EditorUiState::showPerformance` (new bool, default false)
- View > Helpers > Performance menu item in `main.cpp`
- Rendering call in `renderFrame` lambda after `renderEnvironmentPanel`

The panel code lives entirely in the `editor` CMake target — `pixel-roguelike` only links `gameplay`, so no performance panel code enters the runtime game build.

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Deferred Issues (out of scope, pre-existing)

**procedural-model-viewer build failure**
- `apps/model_viewer/main.cpp:6`: `fatal error: 'game/levels/GameAssets.h' file not found`
- Pre-existing before this plan (verified by stashing changes)
- Does not affect `pixel-roguelike` or `level-editor` targets

## Self-Check: PASSED

Files verified:
- FOUND: src/editor/ui/EditorPerformancePanel.h
- FOUND: src/editor/ui/EditorPerformancePanel.cpp
- FOUND: commit 43def06 (task 1)
- FOUND: commit 0af2746 (task 2)
