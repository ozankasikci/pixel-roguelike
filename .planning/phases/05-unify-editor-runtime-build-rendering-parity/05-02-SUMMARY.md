---
phase: 05-unify-editor-runtime-build-rendering-parity
plan: 02
subsystem: rendering
tags: [opengl, editor, viewport, csm, bloom, ssao, ltc, post-process, environment-panel]

# Dependency graph
requires:
  - phase: 05-01
    provides: SceneRenderPipeline class with full Phase 4 pipeline (bloom, SSAO, CSM, LTC area lights)
provides:
  - EditorViewportRenderer class composing SceneRenderPipeline for editor edit-mode
  - Editor viewport now has full Phase 4 rendering parity with runtime
  - CSM cascade split blend (csmLambda) parameter exposed in environment panel
  - PSSM-based cascade split computation replacing hardcoded splits
affects: [05-03, editor-rendering, level-editor-main]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "EditorViewportRenderer wraps SceneRenderPipeline: editor passes pre-collected objects/lights, renderer handles full pipeline"
    - "PSSM lambda blending in CascadedShadowMap::computeCascades: lerp(linear, log, lambda)"
    - "csmLambda flows from PostProcessParams -> SceneRenderInput -> computeCascades"

key-files:
  created:
    - src/editor/render/EditorViewportRenderer.h
    - src/editor/render/EditorViewportRenderer.cpp
  modified:
    - apps/level_editor/main.cpp
    - src/editor/CMakeLists.txt
    - src/engine/rendering/post/PostProcessParams.h
    - src/engine/rendering/lighting/CascadedShadowMap.h
    - src/engine/rendering/lighting/CascadedShadowMap.cpp
    - src/engine/rendering/SceneRenderPipeline.cpp
    - src/editor/ui/EditorEnvironmentPanel.cpp
    - src/game/rendering/EnvironmentDefinition.cpp

key-decisions:
  - "EditorViewportRenderer does NOT own MaterialTextureLibrary — objects arrive pre-resolved with materials from main.cpp's library"
  - "Only finalFbo remains in main.cpp after refactor — SceneRenderPipeline manages its own internal FBOs"
  - "PSSM lambda default 0.5 (balanced) replaces hardcoded splits (0-5m, 5-20m, 20-far)"
  - "csmLambda serialized as csm_lambda in environment asset files for persistence"

patterns-established:
  - "Pattern: Editor viewport renderer as thin adapter over SceneRenderPipeline — no rendering logic duplicated"

requirements-completed: []

# Metrics
duration: 6min
completed: 2026-03-30
---

# Phase 05 Plan 02: Editor Viewport Full Pipeline Parity Summary

**EditorViewportRenderer class extracts ~90 lines of inline editor rendering into SceneRenderPipeline composition, giving the editor edit-mode full Phase 4 bloom/SSAO/CSM/LTC parity with runtime**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-30T16:40:00Z
- **Completed:** 2026-03-30T16:46:15Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Created `EditorViewportRenderer` class that composes `SceneRenderPipeline` for the editor's edit-mode viewport
- Removed ~90 lines of inline shader/shadow/composite/stylize code from `apps/level_editor/main.cpp`
- Editor edit-mode now renders with full Phase 4 features: bloom, SSAO, CSM directional shadows, LTC area lights
- Added `csmLambda` field to `PostProcessParams` with PSSM split blending in `CascadedShadowMap::computeCascades`
- Exposed CSM Split Blend slider in environment panel with tooltip explaining the linear-to-log spectrum

## Task Commits

Each task was committed atomically:

1. **Task 1: Create EditorViewportRenderer and wire into editor main.cpp** - `b08ca05` (feat)
2. **Task 2: Extend environment panel with Phase 4 controls and per-effect toggles** - `33db9e5` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `src/editor/render/EditorViewportRenderer.h` - New editor viewport renderer class wrapping SceneRenderPipeline
- `src/editor/render/EditorViewportRenderer.cpp` - Render implementation mapping EnvironmentDefinition to SceneRenderInput
- `apps/level_editor/main.cpp` - Replaced inline rendering block with EditorViewportRenderer, removed 3 inline shaders/FBOs
- `src/editor/CMakeLists.txt` - Added EditorViewportRenderer.cpp to editor library sources
- `src/engine/rendering/post/PostProcessParams.h` - Added csmLambda field (PSSM cascade split blend)
- `src/engine/rendering/lighting/CascadedShadowMap.h` - Added lambda parameter to computeCascades signature
- `src/engine/rendering/lighting/CascadedShadowMap.cpp` - Implemented PSSM lerp(linear, log, lambda) split computation
- `src/engine/rendering/SceneRenderPipeline.cpp` - Wired postParams->csmLambda into csmShadowMap_.computeCascades()
- `src/editor/ui/EditorEnvironmentPanel.cpp` - Added CSM Split Blend DragFloat slider with tooltip
- `src/game/rendering/EnvironmentDefinition.cpp` - Added csm_lambda parse/serialize for environment asset files

## Decisions Made
- EditorViewportRenderer does NOT own a MaterialTextureLibrary — objects arrive pre-collected and material-resolved by main.cpp's existing library. This avoids duplication and keeps the clear separation: main.cpp collects, EditorViewportRenderer renders.
- Only `finalFbo` remains in main.cpp after refactor. SceneRenderPipeline manages sceneFBO and compositeFBO internally via `ensureFramebuffers()`.
- PSSM default lambda is 0.5 (balanced linear+log) replacing the old hardcoded fixed splits (0-5m, 5-20m, 20-far). The old hardcoded splits were biased for specific scene scales; PSSM adapts to near/far plane values.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added csm_lambda serialization to EnvironmentDefinition**
- **Found during:** Task 2 (adding csmLambda to PostProcessParams)
- **Issue:** csmLambda field added to PostProcessParams but not serialized — field value would be lost when saving environment assets
- **Fix:** Added `csm_lambda` parse record in `loadEnvironmentDefinitionAsset` and `writeFloat` call in `serializeEnvironmentDefinitionAsset`
- **Files modified:** src/game/rendering/EnvironmentDefinition.cpp
- **Verification:** Build succeeded; serialization follows existing pattern of all other PostProcessParams fields
- **Committed in:** 33db9e5 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Essential for data persistence correctness. No scope creep.

## Issues Encountered
None — all tasks executed smoothly. The plan's architecture matched the existing code structure accurately.

## Known Stubs
None — editor viewport rendering is fully wired to SceneRenderPipeline with real bloom, SSAO, CSM, and LTC data.

## Next Phase Readiness
- EditorViewportRenderer is ready for use by Plan 03 (editor asset preview renderer LTC integration per D-12)
- `ltcData()` accessor on EditorViewportRenderer exposes the pipeline's LTC lookup textures for the asset preview renderer
- Environment panel controls (bloom, SSAO, CSM) now reflect live in the editor viewport
- All three executables build cleanly: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`

---
*Phase: 05-unify-editor-runtime-build-rendering-parity*
*Completed: 2026-03-30*
