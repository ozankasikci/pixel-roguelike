---
phase: 04-improve-lighting-quality
plan: 01
subsystem: rendering
tags: [opengl, glsl, shadows, pcf, poisson-disk, spot-lights]

# Dependency graph
requires: []
provides:
  - 16-tap rotated Poisson disk PCF shadow sampling in scene.frag
  - 8 simultaneous shadow-casting spot lights (expanded from 2)
  - macOS-safe explicit if/else sampler indexing for all 8 shadow slots
affects:
  - 04-02 (ambient occlusion)
  - 04-03 (bloom improvements)
  - any plan that adds shadow-casting spot lights to scenes

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Poisson disk PCF with per-fragment rotation for soft shadow edges"
    - "Explicit if/else sampler indexing for macOS OpenGL driver compatibility (no dynamic uShadowMaps[i] indexing)"
    - "kMaxShadowedSpotLights constant drives both C++ array sizes and GLSL uniform array sizes"

key-files:
  created: []
  modified:
    - assets/shaders/game/scene.frag
    - src/engine/rendering/lighting/RenderLight.h
    - src/engine/rendering/geometry/Renderer.h
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/editor/render/EditorScenePreviewRenderer.h
    - src/editor/render/EditorScenePreviewRenderer.cpp

key-decisions:
  - "Explicit if/else chains (not dynamic array indexing) for all shadow sampler accesses — macOS OpenGL driver requirement"
  - "16-tap Poisson disk with per-fragment hash rotation preferred over 9-tap box PCF for banding-free soft shadows"
  - "spread=3.0 for Poisson disk kernel gives moderate softness matching Stanley Parable aesthetic"
  - "ShadowRenderData initializers changed from hardcoded 2-element lists to fill() / loop for maintainability"

patterns-established:
  - "Shadow capacity changes: update kMaxShadowedSpotLights in RenderLight.h only — ShadowRenderData, RuntimeSceneRenderer arrays, and EditorScenePreviewRenderer all pick it up"
  - "New shadow maps: bind to texture units 8+ (8 through 15 for 8 shadow maps), material maps at 12-15"

requirements-completed: []

# Metrics
duration: 18min
completed: 2026-03-30
---

# Phase 04 Plan 01: Soft Shadows Summary

**16-tap rotated Poisson disk PCF shadow sampling with 8 simultaneous shadow-casting spot lights, replacing 9-tap box PCF**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-03-30T00:57:00Z
- **Completed:** 2026-03-30T01:15:21Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- Shadow quality upgraded from 3x3 box PCF (9 samples, visible banding) to 16-tap rotated Poisson disk PCF (smooth, diffuse edges)
- Per-fragment hash rotation eliminates temporal banding patterns that box PCF produces
- Shadow slot capacity expanded from 2 to 8 simultaneous shadow-casting spot lights
- All shadow sampler access uses explicit if/else chains (indices 0-7) for macOS OpenGL driver compatibility

## Task Commits

1. **Task 1: Expand shadow infrastructure from 2 to 8 slots** - `6722726` (feat)
2. **Task 2: Replace 3x3 box PCF with 16-tap rotated Poisson disk in scene.frag** - `ce63aaf` (feat)

## Files Created/Modified

- `assets/shaders/game/scene.frag` - uShadowMaps[8]/uShadowMatrices[8] uniforms; 0-7 explicit if/else in shadowTexelSize, shadowDepthAt, shadowClipPosition; poissonDisk[16] array; hash12() function; 16-tap Poisson PCF body replacing 3x3 box filter
- `src/engine/rendering/lighting/RenderLight.h` - kMaxShadowedSpotLights 2 → 8
- `src/engine/rendering/geometry/Renderer.h` - ShadowRenderData arrays use fill()/default init instead of hardcoded 2-element initializers
- `src/game/rendering/RuntimeSceneRenderer.h` - kMaxShadowedSpotLights 2 → 8 (shadowMaps_ array expands to 8 automatically)
- `src/game/rendering/RuntimeSceneRenderer.cpp` - renderShadowPass initializes all 8 shadow map slots via fill() and loop
- `src/editor/render/EditorScenePreviewRenderer.h` - renderShadowPass signature uses kMaxShadowedSpotLights
- `src/editor/render/EditorScenePreviewRenderer.cpp` - local constant updated, renderShadowPass uses fill() and loop

## Decisions Made

- Used `hash12(gl_FragCoord.xy)` for per-fragment rotation — cheap, spatially uncorrelated, no texture lookup needed
- `spread = 3.0` texels for Poisson kernel — moderate softness, not overly blurred (matches Stanley Parable's gentle shadow style)
- Kept explicit if/else for all 8 indices (not 7 + default fallback for index 7) to be consistent with the documented macOS driver safety requirement

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Updated ShadowRenderData struct initializers in Renderer.h**
- **Found during:** Task 1
- **Issue:** ShadowRenderData had hardcoded `{glm::mat4(1.0f), glm::mat4(1.0f)}` and `{0, 0}` initializers — these would be invalid initializer lists once the array size changed to 8
- **Fix:** Changed to `fill(glm::mat4(1.0f))` and loop-based initialization in renderShadowPass, default-init in struct
- **Files modified:** src/engine/rendering/geometry/Renderer.h, src/game/rendering/RuntimeSceneRenderer.cpp
- **Verification:** Build succeeds
- **Committed in:** 6722726 (Task 1 commit)

**2. [Rule 2 - Missing Critical] Updated EditorScenePreviewRenderer to match new slot count**
- **Found during:** Task 1
- **Issue:** EditorScenePreviewRenderer had a local constant `kMaxShadowedSpotLightsLocal = 2` and hardcoded `std::array<ShadowMap, 2>` — would be out of sync and break the shadow assignment logic
- **Fix:** Changed local constant to reference `kMaxShadowedSpotLights`, updated function signature to `std::array<ShadowMap, kMaxShadowedSpotLights>`, fixed initializers to use fill() and loop
- **Files modified:** src/editor/render/EditorScenePreviewRenderer.h, src/editor/render/EditorScenePreviewRenderer.cpp
- **Verification:** Build succeeds
- **Committed in:** 6722726 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 2 — missing critical consistency updates)
**Impact on plan:** Both auto-fixes needed for correctness — hardcoded array sizes would produce UB or wrong shadow slot assignment. No scope creep.

## Issues Encountered

- Initial build used parallel jobs and hit a macOS ranlib race condition on the static library link step. Resolved by using `-j1` on the build command. Not a code issue.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Shadow quality foundation complete; 8 shadow slots available for scene designers
- Plan 02 (SSAO) and Plan 03 (bloom) can proceed independently — no shadow dependencies
- Scenes can now assign `castsShadows = true` to up to 8 spot lights without hitches

---
*Phase: 04-improve-lighting-quality*
*Completed: 2026-03-30*
