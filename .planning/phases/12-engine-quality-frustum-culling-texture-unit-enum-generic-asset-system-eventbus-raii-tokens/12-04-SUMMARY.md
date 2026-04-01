---
phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens
plan: 04
subsystem: engine-rendering
tags: [frustum-culling, mesh-discovery, scene-render-pipeline, aabb, gribb-hartmann]

requires:
  - phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens
    plan: 03
    provides: ProceduralGameAssets with file-alias registrations removed, GenericFileScene with registerAssets lambda ready for auto-discovery

provides:
  - File-based mesh auto-discovery wired into all three asset loading sites (GenericFileScene, RuntimeGameSession, EditorPreviewWorld)
  - FrustumCulling.h/cpp with Gribb-Hartmann plane extraction and AABB world-space test
  - AABB frustum culling active in SceneRenderPipeline for all render paths
  - SceneRenderPipelineStats.culledCount reports objects skipped per frame
  - drawCalls stat now reflects post-cull count, not total object count

affects:
  - any future plan that reads SceneRenderPipelineStats (drawCalls semantics changed)
  - any plan adding new mesh files to assets/meshes/ (auto-discovered, no code needed)

tech-stack:
  added: []
  patterns:
    - "Gribb-Hartmann VP matrix frustum plane extraction — 6 planes from VP columns, no normalization needed for inclusion test"
    - "AABB frustum cull: transform 8 local corners to world space via modelMatrix, test each against 6 planes — fully outside any plane = cull"
    - "culledInput pattern: copy SceneRenderInput, point objects to culledObjects vector, pass culledInput to all sub-passes"
    - "Mesh auto-discovery: discoverProjectAssets after registerProceduralAssets so procedural meshes take priority via has() check"

key-files:
  created:
    - src/engine/rendering/FrustumCulling.h
    - src/engine/rendering/FrustumCulling.cpp
  modified:
    - src/game/scenes/GenericFileScene.cpp
    - src/engine/rendering/SceneRenderPipeline.cpp
    - src/engine/rendering/SceneRenderPipeline.h
    - src/engine/CMakeLists.txt

key-decisions:
  - "culledInput pattern (option c): copy SceneRenderInput and swap objects pointer to culledObjects — cleanest way to thread culled list through renderShadowPass, renderScenePass, renderPostProcess without changing their signatures"
  - "Objects with null mesh pointer are kept in culledObjects for safety — avoids null deref in shadow/scene passes"
  - "drawCalls stat now reflects post-cull object count; objectCount retains full pre-cull count — both needed for profiling"
  - "D-03 (light culling) deferred per research recommendation — 32-light cap makes uniform upload negligible vs draw call reduction"

patterns-established:
  - "Add mesh files to assets/meshes/ — they are auto-discovered at startup with no code changes required"
  - "FrustumCulling.h is a standalone utility with no class or global state — include and call directly"

requirements-completed: []

duration: 15min
completed: 2026-04-01
---

# Phase 12 Plan 04: Mesh Auto-Discovery and AABB Frustum Culling Summary

**Gribb-Hartmann AABB frustum culling in SceneRenderPipeline and file-based mesh auto-discovery in all three asset loading sites — new meshes in assets/meshes/ require no code registration**

## Performance

- **Duration:** 15 min
- **Started:** 2026-04-01T18:20:00Z
- **Completed:** 2026-04-01T18:35:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Wired `discoverProjectAssets` into GenericFileScene.cpp's `registerAssets` lambda — the last loading site that lacked auto-discovery
- Created FrustumCulling.h/cpp: `extractFrustumPlanes` (Gribb-Hartmann) and `isAabbInsideFrustum` (8 world-space corners test)
- Integrated culling into SceneRenderPipeline::render(): culledObjects built before shadow/scene passes; culledInput threads it through all sub-passes
- SceneRenderPipelineStats gains `culledCount`; `drawCalls` now reports post-cull count

## Task Commits

1. **Task 1: Wire file-based mesh auto-discovery into GenericFileScene** - `81605ff` (feat)
2. **Task 2: Implement AABB frustum culling in SceneRenderPipeline** - `20152a1` (feat)

## Files Created/Modified
- `src/game/scenes/GenericFileScene.cpp` - Added ModelLoader.h/PathUtils.h includes; registerAssets lambda calls discoverProjectAssets after registerProceduralAssets
- `src/engine/rendering/FrustumCulling.h` - extractFrustumPlanes + isAabbInsideFrustum declarations
- `src/engine/rendering/FrustumCulling.cpp` - Gribb-Hartmann implementation; transforms local AABB 8 corners to world space before plane test
- `src/engine/rendering/SceneRenderPipeline.cpp` - Frustum culling integrated into render(); culledInput pattern used for all sub-passes
- `src/engine/rendering/SceneRenderPipeline.h` - Added culledCount field to SceneRenderPipelineStats
- `src/engine/CMakeLists.txt` - FrustumCulling.cpp added to engine_rendering target

## Decisions Made
- Used `culledInput = input; culledInput.objects = &culledObjects` pattern (option c from plan) — cleanest way to thread culled list without changing sub-pass signatures
- Objects with null mesh pointer kept in culledObjects for safety — avoids null deref in shadow/scene passes for any render objects that lack geometry
- `drawCalls` now post-cull count; `objectCount` retains total for profiling comparison
- Light culling (D-03) skipped per research recommendation — 32-light cap makes it negligible

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Worktree was on an older branch predating Plan 03 changes (ProceduralGameAssets rename). Resolved by merging main into the worktree before executing Task 1.

## Next Phase Readiness
- Phase 12 complete — all 4 plans executed
- FrustumCulling is a standalone utility available for any future render path
- New mesh assets dropped into assets/meshes/ are auto-discovered across all three loading contexts

---

## Self-Check

- [x] `src/engine/rendering/FrustumCulling.h` exists
- [x] `src/engine/rendering/FrustumCulling.cpp` exists
- [x] GenericFileScene.cpp contains `discoverProjectAssets`
- [x] SceneRenderPipeline.cpp contains `extractFrustumPlanes(vp)`
- [x] SceneRenderPipeline.cpp contains `isAabbInsideFrustum`
- [x] SceneRenderPipeline.cpp contains `culledObjects`
- [x] SceneRenderPipeline.h contains `culledCount`
- [x] Build succeeds with zero errors for pixel-roguelike and level-editor

## Self-Check: PASSED

---
*Phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens*
*Completed: 2026-04-01*
