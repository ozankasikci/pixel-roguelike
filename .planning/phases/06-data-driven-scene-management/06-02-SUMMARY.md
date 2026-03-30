---
phase: 06-data-driven-scene-management
plan: 02
subsystem: build
tags: [project-config, runtime-startup, editor-startup, scene-picker, imgui]

# Dependency graph
requires:
  - phase: 06-data-driven-scene-management
    plan: 01
    provides: readProjectCfgLastScene / writeProjectCfgLastScene utilities in engine_core
provides:
  - Runtime three-tier scene resolution: --scene > project.cfg > ImGui first-launch picker
  - Editor reads project.cfg on startup, writes on every scene open
  - Editor graceful empty-state with No scene loaded overlay and New/Open buttons
  - No hardcoded scene paths remain in runtime or editor
affects:
  - 06-03 (New Scene dialog will wire into the "New Scene..." placeholder button in empty state)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Three-tier scene resolution in runtime (CLI arg > project.cfg > picker)
    - Pre-loop ImGuiLayer for picker (standalone lifecycle before app.run / RenderSystem init)
    - Startup operation gating on non-empty initialScene (Pitfall 3 pattern)

key-files:
  created: []
  modified:
    - apps/runtime/main.cpp
    - apps/level_editor/main.cpp
    - src/editor/ui/LevelEditorUi.h

key-decisions:
  - "Pre-loop ImGuiLayer for scene picker is safe — RenderSystem initializes its own ImGuiLayer in init() called during app.run(), picker runs before app.run() so no double-init conflict"
  - "Empty-state guard on rendering blocks (previewWorld.rebuild, runtimePreviewSession.rebuild, editorViewportRenderer.render) uses ui.pendingScenePath.empty() as the single sentinel"
  - "New Scene popup is a placeholder — fully wired in Plan 03 per plan instructions"

patterns-established:
  - "ui.pendingScenePath.empty() is the single empty-state sentinel — all scene-dependent rendering guards on this"
  - "writeProjectCfgLastScene called immediately after loadSceneIntoEditor — project.cfg always up-to-date after any scene open"

requirements-completed: []

# Metrics
duration: 10min
completed: 2026-03-30
---

# Phase 06 Plan 02: Runtime and Editor project.cfg Wiring Summary

**Three-tier scene resolution in runtime (--scene > project.cfg > ImGui picker) and project.cfg read/write in editor with graceful empty-state overlay when no scene is loaded**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-30T18:40:00Z
- **Completed:** 2026-03-30T18:50:00Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Added `listAvailableScenes()` to runtime main for ImGui first-launch picker with selectable list and Launch button
- Runtime now resolves scene via `--scene` arg, then `project.cfg last_scene`, then shows ImGui picker for first launch
- Editor reads `project.cfg` at startup using `readProjectCfgLastScene()` and auto-loads last scene
- Editor writes `project.cfg` after every `loadSceneIntoEditor()` call via `writeProjectCfgLastScene()`
- Editor handles no-scene state gracefully: startup gated on `!initialScene.empty()`, empty-state overlay shows "No scene loaded" with New Scene / Open Scene buttons
- All viewport rendering (previewWorld, runtimePreviewSession, editorViewportRenderer) guarded by `!ui.pendingScenePath.empty()`
- Cleared hardcoded `pendingScenePath = "assets/scenes/silos_cloister.scene"` default from `EditorUiState`

## Task Commits

1. **Task 1: Wire project.cfg into runtime startup with scene picker fallback** — `e4a9a9b` (feat)
2. **Task 2: Wire project.cfg into editor startup, write-back on scene open, handle empty state** — `3b82abf` (feat)

**Plan metadata:** (included in final docs commit)

## Files Created/Modified

- `apps/runtime/main.cpp` — Added three-tier scene resolution: --scene > project.cfg > ImGui picker; added `listAvailableScenes()` helper; picker lifecycle is standalone pre-loop
- `apps/level_editor/main.cpp` — Added project.cfg read at startup; gated all startup scene ops on non-empty initialScene; added writeProjectCfgLastScene after loadSceneIntoEditor; added empty-state overlay; guarded scene rendering blocks
- `src/editor/ui/LevelEditorUi.h` — Cleared hardcoded `pendingScenePath` default (was silos_cloister.scene, now empty string)

## Decisions Made

- Pre-loop `ImGuiLayer` for the runtime scene picker is safe: `RenderSystem::init()` creates its own `ImGuiLayer` inside `app.run()`, so the picker's temporary ImGuiLayer runs before and is shut down before any conflict arises
- `ui.pendingScenePath.empty()` used as the single empty-state sentinel across all scene-dependent rendering guards — avoids redundant tracking state
- "New Scene..." popup is a Plan 03 placeholder — added stub popup with explanatory text per plan instructions

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Both executables (pixel-roguelike, level-editor) compiled cleanly on first build attempt.

## Known Stubs

- `NewScenePopup` in the editor empty-state overlay: opens a popup saying "New Scene will be available in Plan 03." — intentional, Plan 03 will fully wire the New Scene dialog

## Next Phase Readiness

- Plan 03 can now focus solely on the New Scene dialog and hooking it into the "New Scene..." button already present in the empty-state overlay
- project.cfg is always written on scene open — runtime and editor share a single source of truth for the default scene
- No hardcoded scene paths remain in runtime or editor entry points

---
*Phase: 06-data-driven-scene-management*
*Completed: 2026-03-30*
