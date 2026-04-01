---
phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens
plan: 01
subsystem: rendering
tags: [opengl, texture-units, eventbus, raii, cpp20]

# Dependency graph
requires: []
provides:
  - "TextureUnits.h namespace with named constants for all scene shader texture unit slots"
  - "EventBus RAII SubscriptionToken with move semantics and [[nodiscard]] subscribe"
  - "Duplicate kMaxShadowedSpotLights removed from SceneRenderPipeline.h"
affects:
  - "12-03-generic-asset-system"
  - "12-04-frustum-culling"
  - "any future subscriber code using EventBus"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "TextureUnits namespace: named constants instead of magic integers for GL texture unit slots"
    - "RAII subscription tokens: EventBus::subscribe() returns move-only token; destructor unsubscribes"
    - "[[nodiscard]] on subscription return forces callers to store the token"

key-files:
  created:
    - "src/engine/rendering/TextureUnits.h"
  modified:
    - "src/engine/rendering/geometry/Renderer.cpp"
    - "src/engine/rendering/SceneRenderPipeline.cpp"
    - "src/engine/rendering/SceneRenderPipeline.h"
    - "src/editor/render/EditorAssetPreviewRenderer.cpp"
    - "src/engine/core/EventBus.h"
    - "src/game/runtime/RuntimeGameSession.cpp"

key-decisions:
  - "TextureUnits uses a namespace (not enum class) to allow implicit int conversion for GL calls"
  - "kShadowMap0 through +7 intentionally overlaps kAlbedo/kNormalMap/kRoughnessMap/kAoMap (12-15) — different uniforms sample them"
  - "EventBus is main-thread-only; no mutex added"
  - "[[nodiscard]] on subscribe() prevents silent token discard (subscription immediately fires and dies)"

patterns-established:
  - "TextureUnits::k prefix for all GL texture unit references across rendering pipeline"
  - "EventBus subscriber must store returned SubscriptionToken in a member to keep subscription alive"

requirements-completed: []

# Metrics
duration: 15min
completed: 2026-04-01
---

# Phase 12 Plan 01: TextureUnits enum and EventBus RAII tokens Summary

**Named texture unit constants via TextureUnits namespace replace all magic integers across the rendering pipeline; EventBus gains move-only RAII SubscriptionToken with [[nodiscard]]**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-04-01T18:15:00Z
- **Completed:** 2026-04-01T18:30:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Created `src/engine/rendering/TextureUnits.h` with all 7 named constants (kShadowMap0=8, kLtcMat=10, kLtcAmp=11, kAlbedo=12, kNormalMap=13, kRoughnessMap=14, kAoMap=15, kCsmShadowMap=16)
- Replaced 9 magic integer usages in Renderer.cpp, 6 in SceneRenderPipeline.cpp, 6 in EditorAssetPreviewRenderer.cpp
- Removed duplicate `static constexpr int kMaxShadowedSpotLights = 8` from SceneRenderPipeline.h
- Rewrote EventBus.h with SubscriptionToken nested class — move-only, RAII unsubscribe on destruction, [[nodiscard]] prevents silent no-op subscriptions

## Task Commits

Each task was committed atomically (both absorbed into parallel plan-02 agent commits):

1. **Task 1: Create TextureUnits.h and replace all magic-number texture unit indices** - `a9e1378` (refactor)
2. **Task 2: Add RAII SubscriptionToken to EventBus** - `a3f6656` (docs, absorbed with plan-02 summary)

## Files Created/Modified
- `src/engine/rendering/TextureUnits.h` - Named constants namespace for all GL texture unit slots used in scene shader
- `src/engine/rendering/geometry/Renderer.cpp` - Shadow map and material texture bindings use TextureUnits:: constants
- `src/engine/rendering/SceneRenderPipeline.cpp` - CSM and LTC bindings use TextureUnits:: constants; removed local constexpr definitions
- `src/engine/rendering/SceneRenderPipeline.h` - Removed duplicate kMaxShadowedSpotLights (kept in RenderLight.h)
- `src/editor/render/EditorAssetPreviewRenderer.cpp` - LTC and CSM bindings use TextureUnits:: constants
- `src/engine/core/EventBus.h` - Full rewrite with SubscriptionToken class, ID-based unsubscribe, [[nodiscard]] subscribe
- `src/game/runtime/RuntimeGameSession.cpp` - Fixed LevelLoader::load() call to use new LevelLoadArgs struct API (deviation fix)

## Decisions Made
- TextureUnits is a `namespace` not `enum class` — allows direct use as int argument to GL and shader functions without casting
- The 8–15 shadow / 12–15 material overlap is preserved intentionally: different GL uniforms (uShadowMaps[] vs uAlbedoMap) sample the overlapping slots
- EventBus is single-threaded only (no mutex); documented with comment at class level
- [[nodiscard]] forces future callers to store token — prevents "subscribe and immediately forget" bugs

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed LevelLoader::load() call site with wrong argument order**
- **Found during:** Task 2 verification build
- **Issue:** `RuntimeGameSession.cpp:144` called `loader.load(content, runSession_, resolvedRequest, level)` but the parallel plan-02 agent had already refactored `LevelLoader::load()` to `(request, args)` signature with `LevelLoadArgs` struct
- **Fix:** Updated call to `LevelLoadArgs args{&content, &runSession_, &level}; loader.load(resolvedRequest, args);`
- **Files modified:** `src/game/runtime/RuntimeGameSession.cpp`
- **Verification:** Both pixel-roguelike and level-editor build cleanly
- **Committed in:** `a3f6656` (absorbed into plan-02 docs commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Fix required for build to succeed. No scope creep.

## Issues Encountered
- Parallel plan-02 agent committed Task 1 (TextureUnits.h + all rendering file updates) and Task 2 (EventBus) changes as part of its own commits. Both sets of changes landed correctly in the repository; per-task attribution is to the parallel agent's commit hashes rather than dedicated plan-01 commits.

## Next Phase Readiness
- TextureUnits.h available for any future shader work needing texture unit references
- EventBus subscription pattern ready for game systems that need lifecycle-safe event handlers
- No blockers for plans 03 and 04

---
*Phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens*
*Completed: 2026-04-01*
