---
phase: 05-unify-editor-runtime-build-rendering-parity
plan: 03
subsystem: rendering
tags: [opengl, SceneRenderPipeline, LtcData, ImGui, model-viewer, asset-preview]

# Dependency graph
requires:
  - phase: 05-01
    provides: SceneRenderPipeline with full Phase 4 rendering (bloom, SSAO, CSM, LTC area lights)

provides:
  - Model viewer (procedural-model-viewer) uses SceneRenderPipeline for rendering
  - SceneRenderPipelineStats struct with per-effect CPU timing
  - Asset preview renderer binds real LTC lookup textures for area light support
  - Editor viewport performance overlay showing frame time and FPS
affects:
  - future plans that add editor rendering features
  - any consumer of SceneRenderPipeline that wants per-effect profiling

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "glfwGetTime() for CPU-side render timing in SceneRenderPipeline::render() and renderPostProcess()"
    - "LtcData reuse pattern: EditorAssetPreviewRenderer owns its own LtcData instance for LTC texture binding"
    - "ImDrawList overlay pattern: stats drawn directly to draw list anchored to viewport origin"

key-files:
  created: []
  modified:
    - src/engine/rendering/SceneRenderPipeline.h
    - src/engine/rendering/SceneRenderPipeline.cpp
    - apps/model_viewer/main.cpp
    - src/editor/render/EditorAssetPreviewRenderer.h
    - src/editor/render/EditorAssetPreviewRenderer.cpp
    - src/editor/ui/LevelEditorUi.h
    - apps/level_editor/main.cpp

key-decisions:
  - "Model viewer replaces inline Renderer/CompositePass/StylizePass with SceneRenderPipeline; shadows disabled by default (no directional lights in viewer)"
  - "SceneRenderPipelineStats uses CPU glfwGetTime() timing (not GPU queries); adequate for editor profiling, no GL sync stall"
  - "EditorAssetPreviewRenderer owns its own LtcData instance rather than borrowing from the pipeline — preview renderer is standalone, not composed through SceneRenderPipeline"
  - "Viewport stats overlay uses ImDrawList (not ImGui child windows) to avoid layout interference with ImGuizmo"
  - "showViewportStats defaults to true and is toggleable via View > Helpers > Viewport Stats"

patterns-established:
  - "SceneRenderPipelineStats: per-effect timing exposed via lastStats() const accessor; consumers read without mutation"
  - "Asset preview LTC binding: use shader->setInt + glBindTexture with explicit unit before drawScene call"

requirements-completed: []

# Metrics
duration: 30min
completed: 2026-03-30
---

# Phase 05 Plan 03: Model Viewer Pipeline + Asset Preview LTC + Viewport Stats Summary

**Model viewer switched to SceneRenderPipeline (full Phase 4 features), asset previews bound to real LTC textures, and per-effect timing stats added to pipeline with editor viewport overlay**

## Performance

- **Duration:** ~30 min
- **Started:** 2026-03-30T16:15:00Z
- **Completed:** 2026-03-30T16:46:00Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- Replaced inline Renderer/CompositePass/StylizePass/FBO setup in model viewer with `SceneRenderPipeline::render()` — model viewer now gets bloom, SSAO, LTC area lights, and CSM automatically
- Added `SceneRenderPipelineStats` struct with per-effect CPU timing (`totalRenderMs`, `shadowPassMs`, `scenePassMs`, `bloomMs`, `ssaoMs`, `compositeMs`, `drawCalls`, `objectCount`, `lightCount`) instrumented via `glfwGetTime()` in `render()` and `renderPostProcess()`
- Asset preview renderer (`EditorAssetPreviewRenderer`) now initializes its own `LtcData` and binds real LTC lookup textures to units 10/11 before `drawScene()`, eliminating the null-texture sampler mismatch that caused area light rendering to be undefined
- Added CSM disable uniforms in asset preview (null `GL_TEXTURE_2D_ARRAY` bound to unit 16) to prevent sampler type errors
- Added viewport performance stats overlay to the level editor viewport (ImDrawList-based, bottom-left corner) showing frame ms, FPS, and play-mode render ms; toggleable via View > Helpers > Viewport Stats

## Task Commits

1. **Task 1: Wire model viewer to SceneRenderPipeline and add pipeline stats** - `3ea03f3`
2. **Task 2: Update asset preview renderer with LTC textures and performance overlay** - `df37b77`

## Files Created/Modified

- `src/engine/rendering/SceneRenderPipeline.h` - Added `SceneRenderPipelineStats` struct, `lastStats_` member, `lastStats()` const accessor
- `src/engine/rendering/SceneRenderPipeline.cpp` - Instrumented `render()` and `renderPostProcess()` with `glfwGetTime()` timing
- `apps/model_viewer/main.cpp` - Replaced inline rendering with `SceneRenderPipeline pipeline; pipeline.init(); pipeline.render(input, ...)`
- `src/editor/render/EditorAssetPreviewRenderer.h` - Added `#include "engine/rendering/lighting/LtcData.h"` and `LtcData ltcData_` member
- `src/editor/render/EditorAssetPreviewRenderer.cpp` - Added `ltcData_.init()` in `ensureInitialized()` and LTC/CSM uniform setup in `renderPreviewMesh()`
- `src/editor/ui/LevelEditorUi.h` - Added `bool showViewportStats = true` to `EditorUiState`
- `apps/level_editor/main.cpp` - Added viewport stats ImDrawList overlay and View > Helpers > Viewport Stats toggle

## Decisions Made

- Used CPU `glfwGetTime()` for timing rather than GL timer queries — avoids GPU sync stalls and is adequate for editor-facing profiling numbers
- `EditorAssetPreviewRenderer` owns its own `LtcData` rather than receiving one from a pipeline — the asset preview renderer is a standalone system that calls `drawScene()` directly, not composed through `SceneRenderPipeline`
- Model viewer sets `shadowsEnabled = false` in `SceneRenderInput` — the viewer has only point lights, no spot lights with `castsShadows`, so shadow rendering would be a no-op anyway
- Viewport stats overlay uses `ImDrawList::AddText` (direct draw) rather than `ImGui::BeginChild` — avoids layout position issues near ImGuizmo handles

## Deviations from Plan

None — plan executed exactly as written. The worktree required a `git merge main` before starting (the branch was behind main by the 05-01 and 05-02 commits), but this is a parallel execution setup detail, not a plan deviation.

## Issues Encountered

The worktree branch `worktree-agent-a7a767d3` was behind `main` by the 05-01 work that created `SceneRenderPipeline`. A `git merge main --no-edit` resolved this cleanly before starting implementation.

## Next Phase Readiness

- All three executables (`pixel-roguelike`, `level-editor`, `procedural-model-viewer`) compile and use the shared `SceneRenderPipeline` (runtime and model viewer directly; editor viewport TBD in a future plan)
- `SceneRenderPipelineStats` is available for any future editor perf panel that wants more granular timing
- Asset previews now have correct LTC texture binding — area light thumbnails will render correctly

## Self-Check: PASSED

- SUMMARY.md: FOUND at .planning/phases/05-unify-editor-runtime-build-rendering-parity/05-03-SUMMARY.md
- Commit 3ea03f3 (Task 1): FOUND
- Commit df37b77 (Task 2): FOUND
- Build: all executables compiled successfully

---
*Phase: 05-unify-editor-runtime-build-rendering-parity*
*Completed: 2026-03-30*
