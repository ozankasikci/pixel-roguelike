---
phase: 04-improve-lighting-quality
plan: 02
subsystem: rendering
tags: [opengl, glsl, bloom, post-process, hdr, mip-chain, engine]

requires:
  - phase: 04-improve-lighting-quality
    provides: Phase 04 context, research on bloom pipeline (D-06, D-07, D-08), SSAO and lighting quality improvements

provides:
  - BloomPass class with 5-level mip-chain downsample/upsample pipeline
  - 13-tap COD-style downsample kernel shader
  - 3x3 tent filter upsample shader
  - Bloom operating in HDR space before ACES tonemapping
  - Configurable bloom intensity/radius per-environment via PostProcessParams
  - Old 8-tap bloomGlow function removed

affects: [04-improve-lighting-quality, game-rendering, post-process]

tech-stack:
  added: []
  patterns:
    - "Mip-chain bloom: 5 half-resolution FBOs (GL_RGBA16F), downsample then additive-blend upsample"
    - "BloomPass owns its own fullscreen quad VAO/VBO (same pattern as CompositePass)"
    - "Bloom texture bound to texture unit 8 in composite pass (units 0-7 were pre-existing)"

key-files:
  created:
    - src/engine/rendering/post/BloomPass.h
    - src/engine/rendering/post/BloomPass.cpp
    - assets/shaders/engine/bloom_downsample.vert
    - assets/shaders/engine/bloom_downsample.frag
    - assets/shaders/engine/bloom_upsample.vert
    - assets/shaders/engine/bloom_upsample.frag
  modified:
    - src/engine/rendering/post/CompositePass.h
    - src/engine/rendering/post/CompositePass.cpp
    - assets/shaders/engine/composite.frag
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/engine/CMakeLists.txt

key-decisions:
  - "BloomPass::resize() always recreates mip FBOs when called from ensureFramebuffers() — simple and correct, minor overhead acceptable"
  - "No threshold extraction in downsample — HDR values naturally dominate bright pixels; bloomThreshold field kept in PostProcessParams for backward compatibility but unused"
  - "bloomRadius * 0.003f as filter radius scaling — existing bloomRadius ~1.8 maps to ~0.005 filter radius for subtle tent spread"
  - "Bloom texture bound to unit 8 in CompositePass (units 0-7 are scene/sky/cloud textures)"

patterns-established:
  - "Multi-pass bloom: BloomPass called between scene pass and composite pass in renderPostProcess()"
  - "Bloom FBOs use GL_RGBA16F (HDR) with GL_LINEAR filtering for smooth mip interpolation"

requirements-completed: []

duration: 13min
completed: 2026-03-30
---

# Phase 04 Plan 02: Bloom Pipeline Summary

**Mip-chain downsample/upsample bloom with 5 GL_RGBA16F levels replaces old 8-tap bloomGlow, operating in HDR before ACES tonemapping**

## Performance

- **Duration:** 13 min
- **Started:** 2026-03-30T10:29:58Z
- **Completed:** 2026-03-30T10:43:00Z
- **Tasks:** 2
- **Files modified:** 12

## Accomplishments

- BloomPass class with 5-level mip chain (half-res, quarter-res, 1/8, 1/16, 1/32), each FBO using GL_RGBA16F
- 13-tap downsample kernel (COD: Advanced Warfare technique) with weights 0.125 / 0.03125 / 0.0625 / 0.125
- 3x3 tent filter upsample with additive blending across mip levels
- Old bloomGlow() 8-tap threshold function removed from composite.frag
- Bloom added via `texture(uBloomTex, vTexCoord).rgb * uBloomIntensity` in HDR space before acesFitted() call
- BloomPass integrated: init() on renderer init, resize() on framebuffer resize, render() before composite

## Task Commits

1. **Task 1: Create BloomPass class and downsample/upsample shaders** - `cdcf95e` (feat)
2. **Task 2: Wire BloomPass into composite pipeline and remove old bloomGlow** - `eb340f3` (feat)

**Plan metadata:** (see final commit below)

## Files Created/Modified

- `src/engine/rendering/post/BloomPass.h` - BloomPass class declaration with kMipCount=5, MipLevel struct
- `src/engine/rendering/post/BloomPass.cpp` - Full mip-chain implementation: init, resize, render, destroy
- `assets/shaders/engine/bloom_downsample.vert` - Standard fullscreen quad vertex shader (#version 410 core)
- `assets/shaders/engine/bloom_downsample.frag` - 13-tap downsample kernel with explicit weights
- `assets/shaders/engine/bloom_upsample.vert` - Standard fullscreen quad vertex shader (#version 410 core)
- `assets/shaders/engine/bloom_upsample.frag` - 3x3 tent filter dividing by 16.0
- `src/engine/rendering/post/CompositePass.h` - apply() gains bloomTex GLuint parameter
- `src/engine/rendering/post/CompositePass.cpp` - bloomTex bound to GL_TEXTURE8, uBloomTex uniform set
- `assets/shaders/engine/composite.frag` - uniform sampler2D uBloomTex added, bloomGlow() removed, bloom via uBloomTex
- `src/game/rendering/RuntimeSceneRenderer.h` - BloomPass.h included, bloomPass_ member added
- `src/game/rendering/RuntimeSceneRenderer.cpp` - bloomPass_.init(), .resize(), .render() wired into pipeline
- `src/engine/CMakeLists.txt` - BloomPass.cpp added to engine_rendering source list

## Decisions Made

- Bloom texture unit 8: units 0-7 were already occupied by scene/sky/cloud textures in CompositePass
- No threshold extraction: HDR downsample naturally emphasizes bright pixels; bloomGlow's threshold logic removed entirely
- bloomRadius * 0.003f scaling: existing PostProcessParams bloomRadius (~1.8) maps to ~0.005 filter radius, matching research guidance of 0.005 default
- BloomPass::resize() always called from ensureFramebuffers(): simpler than tracking size state, acceptable overhead

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

The main build directory (`/Users/ozan/Projects/gsd-3d-roguelike/build`) references the main branch source, not the worktree branch. The verification was done by direct compilation of changed files using the main build system's include paths and flags, confirming clean compilation of BloomPass.cpp, RuntimeSceneRenderer.cpp, and CompositePass.cpp.

## Known Stubs

None - all bloom pipeline is fully wired. bloomRadius and bloomIntensity from PostProcessParams drive the bloom behavior. No placeholder values in the rendering path.

## Next Phase Readiness

- BloomPass is ready for consumption by all phases in wave 1
- Bloom mip-chain provides the soft warm glow foundation referenced by D-06/D-07
- Plan 04-03 (SSAO) can proceed independently — it uses a similar post-process FBO pattern
- bloomThreshold field in PostProcessParams still exists but has no effect (HDR downsample replaced it) — can be removed in a cleanup phase

---
*Phase: 04-improve-lighting-quality*
*Completed: 2026-03-30*
