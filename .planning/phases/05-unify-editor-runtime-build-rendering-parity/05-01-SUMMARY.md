---
phase: 05-unify-editor-runtime-build-rendering-parity
plan: 01
subsystem: rendering
tags: [opengl, scene-pipeline, ecs, refactoring, engine-rendering, post-process, csm, ssao, bloom, ltc]

# Dependency graph
requires:
  - phase: 04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting
    provides: "Full rendering pipeline with CSM, SSAO, bloom, LTC area lights in RuntimeSceneRenderer"
provides:
  - "SceneRenderPipeline class in engine_rendering — shared orchestrator for shadow pass, CSM, scene pass, bloom, SSAO, composite, stylize"
  - "SceneRenderInput struct — engine-layer-only input type for the pipeline"
  - "RuntimeSceneRenderer refactored as thin ECS adapter composing SceneRenderPipeline"
affects:
  - 05-02  # editor scene renderer will consume SceneRenderPipeline
  - 05-03  # model viewer can upgrade to use SceneRenderPipeline
  - future-editor-preview  # editor preview sessions use the shared pipeline

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pipeline extraction: move rendering orchestration from game-layer class into engine-layer class using composition"
    - "SceneRenderInput as a data-only struct bridging ECS collection (game layer) and pipeline execution (engine layer)"
    - "viewmodelObjects field in SceneRenderInput for depth-range trick — pipeline owns glDepthRange but caller supplies objects"

key-files:
  created:
    - src/engine/rendering/SceneRenderPipeline.h
    - src/engine/rendering/SceneRenderPipeline.cpp
  modified:
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/engine/CMakeLists.txt
    - apps/model_viewer/main.cpp

key-decisions:
  - "SceneRenderPipeline lives in engine_rendering with NO game-layer headers — enforces layering constraint from D-06"
  - "viewmodelObjects added to SceneRenderInput so pipeline handles glDepthRange trick internally, keeping RSR as a pure ECS adapter"
  - "syncSkySunFromDirectional inlined inside SceneRenderPipeline::renderPostProcess using lightingEnvironment sun fields — avoids game-layer dependency"
  - "safeNormalize duplicated as file-local in both SceneRenderPipeline.cpp and RuntimeSceneRenderer.cpp — trivial helper, cleaner than shared header"

patterns-established:
  - "Engine pipeline takes SceneRenderInput (POD struct, engine types only) — game layer builds it from ECS, engine layer consumes it"
  - "Pipeline accessor sceneFBO() exposed for consumers needing rendered pixel access (screenshots, overlays)"
  - "ltcData() accessor exposed for consumers binding LTC textures to other shaders (per D-12)"

requirements-completed: []

# Metrics
duration: 6min
completed: 2026-03-30
---

# Phase 05 Plan 01: SceneRenderPipeline Extraction Summary

**SceneRenderPipeline extracted from RuntimeSceneRenderer into engine_rendering with shadow pass, CSM, scene pass (with viewmodel depth-range trick), bloom, SSAO, composite, and stylize — RSR reduced to a thin ECS adapter**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-30T16:31:54Z
- **Completed:** 2026-03-30T16:37:25Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Created `SceneRenderPipeline` class in `engine_rendering` with full rendering pipeline: spot shadow pass, CSM directional shadow pass, scene FBO pass, bloom, SSAO, composite, stylize — zero game-layer includes in header
- Created `SceneRenderInput` struct as the bridge between ECS-collected data (game layer) and pipeline execution (engine layer)
- Refactored `RuntimeSceneRenderer` from 625-line monolith to ~360-line thin adapter — removed 11 private members and 8 private methods, delegating all pipeline work to `SceneRenderPipeline`
- Fixed pre-existing build error in model viewer where `CompositePass::apply()` was called with wrong argument count

## Task Commits

Each task was committed atomically:

1. **Task 1: Create SceneRenderPipeline class in engine_rendering** - `9d841b3` (feat)
2. **Task 2: Refactor RuntimeSceneRenderer to compose SceneRenderPipeline** - `da6f790` (refactor)

## Files Created/Modified

- `src/engine/rendering/SceneRenderPipeline.h` - Engine-layer pipeline class + SceneRenderInput struct
- `src/engine/rendering/SceneRenderPipeline.cpp` - Pipeline implementation: shadows, CSM, scene, post-process chain
- `src/engine/CMakeLists.txt` - Added SceneRenderPipeline.cpp to engine_rendering sources
- `src/game/rendering/RuntimeSceneRenderer.h` - Stripped to ECS adapter interface; adds pipeline_ member
- `src/game/rendering/RuntimeSceneRenderer.cpp` - Reduced to ECS collection + SceneRenderInput construction + delegation
- `apps/model_viewer/main.cpp` - Bug fix: CompositePass::apply() called with missing bloomTex and ssaoTex args

## Decisions Made

- `SceneRenderPipeline` strictly engine-layer (no game headers) to enforce layering from D-06; game-layer concepts like LightingEnvironment and RenderLight are already in engine_rendering
- `viewmodelObjects` added to `SceneRenderInput` so pipeline handles the glDepthRange(0, 0.01) viewmodel depth trick — caller supplies objects, pipeline applies the trick; this avoids a two-phase render with external FBO reads
- `syncSkySunFromDirectional` is a game-layer function (uses DebugParams); inlined the equivalent logic inside pipeline's `renderPostProcess` using `input.lightingEnvironment.sun` — no game dependency needed
- `safeNormalize` defined as file-local in both `SceneRenderPipeline.cpp` and `RuntimeSceneRenderer.cpp` — 3-line helper, duplication cleaner than introducing a shared header for one utility

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] model viewer CompositePass::apply() missing bloom/SSAO arguments**
- **Found during:** Task 2 (build verification of all three executables)
- **Issue:** `apps/model_viewer/main.cpp` called `compositePass.apply()` with 7 arguments but the function signature requires 9 (bloomTex and ssaoTex added in Phase 4)
- **Fix:** Added `0, 0` for both missing texture arguments (model viewer does not use bloom or SSAO)
- **Files modified:** `apps/model_viewer/main.cpp`
- **Verification:** `cmake --build .` succeeds for all three executables with 0 errors
- **Committed in:** `da6f790` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — pre-existing bug)
**Impact on plan:** Bug pre-dated this plan but blocked build verification. Fix is correct and minimal — no scope creep.

## Issues Encountered

- Missing `#include <glm/gtc/matrix_transform.hpp>` in SceneRenderPipeline.cpp caused `glm::lookAt` and `glm::perspective` to be undefined. Added include and `engine/rendering/geometry/Mesh.h` for `mesh->draw()`. Resolved in first build attempt.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `SceneRenderPipeline` is ready to be consumed by the editor scene renderer (plan 05-02) and model viewer
- Editor preview renderer can call `pipeline_.render(input, ...)` without duplicating shadow/bloom/SSAO/CSM code
- `sceneFBO()` accessor available for any consumer needing the rendered color/depth textures
- `ltcData()` accessor available for asset preview renderer to bind LTC textures

## Self-Check: PASSED

All files confirmed present and commits verified in git history.

---
*Phase: 05-unify-editor-runtime-build-rendering-parity*
*Completed: 2026-03-30*
