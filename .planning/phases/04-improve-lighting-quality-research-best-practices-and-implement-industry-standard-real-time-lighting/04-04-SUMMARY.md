---
phase: 04-improve-lighting-quality
plan: 04
subsystem: rendering
tags: [opengl, glsl, csm, cascaded-shadow-maps, shadows, texture-array, geometry-shader, engine]

requires:
  - phase: 04-improve-lighting-quality
    plan: 01
    provides: ShadowMap class, spot shadow PCF infrastructure, poissonDisk array in scene.frag
  - phase: 04-improve-lighting-quality
    plan: 03
    provides: SsaoPass, Framebuffer MRT setup, scene.frag geometry normal output

provides:
  - CascadedShadowMap class with GL_TEXTURE_2D_ARRAY (3 cascades, 1024x1024)
  - CSM depth shaders with geometry shader for single-pass multi-layer rendering
  - Shader class geometry shader support (vert+geom+frag constructor and load factory)
  - CSM integrated in RuntimeSceneRenderer shadow pass and scene pass
  - sampler2DArrayShadow CSM sampling in scene.frag with 16-tap Poisson PCF
  - Per-cascade bias and 10% cascade blend zone in scene.frag

affects:
  - 04-05-PLAN (area lights / final pass — shares scene.frag and renderer)

tech-stack:
  added: []
  patterns:
    - "CascadedShadowMap follows ShadowMap pattern: create/destroy/bind/unbind with RAII"
    - "glFramebufferTexture (not glFramebufferTexture2D) for attaching all array layers at once"
    - "Geometry shader invocations=3: single draw call renders to all 3 cascade layers via gl_Layer"
    - "CSM uses texture unit 16 (above material textures at 12-15 and spot shadow maps at 8-15)"
    - "CameraState passed to renderShadowPass for cascade computation"

key-files:
  created:
    - src/engine/rendering/lighting/CascadedShadowMap.h
    - src/engine/rendering/lighting/CascadedShadowMap.cpp
    - assets/shaders/engine/csm_depth.vert
    - assets/shaders/engine/csm_depth.geom
    - assets/shaders/engine/csm_depth.frag
  modified:
    - src/engine/rendering/core/Shader.h
    - src/engine/rendering/core/Shader.cpp
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - assets/shaders/game/scene.frag
    - src/engine/CMakeLists.txt

key-decisions:
  - "Shader geometry shader support added as new constructor overload and Shader::load factory — consistent with existing 2-arg pattern"
  - "CSM texture unit 16 avoids conflicts with spot shadow maps (8-15) and material textures (12-15)"
  - "CameraState passed to renderShadowPass rather than storing camera separately — keeps cascade computation co-located with rendering"
  - "CSM applied to all LIGHT_DIRECTIONAL lights in scene.frag — when uCsmEnabled=0, sampleCsmShadow returns 1.0 (no shadow), so fill light is unaffected"
  - "Fixed cascade splits (5m, 20m, far) chosen for institutional indoor/outdoor scenes — close contact, mid-range, distant"

requirements-completed: []

duration: ~5min
completed: 2026-03-30
---

# Phase 04 Plan 04: Cascaded Shadow Maps (CSM) Summary

**3-cascade CSM for directional sun light using GL_TEXTURE_2D_ARRAY with geometry shader single-pass rendering, 16-tap Poisson PCF, per-cascade bias, and 10% cascade blend zones**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-03-30T00:00:00Z
- **Completed:** 2026-03-30T00:05:00Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- Added geometry shader support to `Shader` class (new 3-arg constructor and `Shader::load` factory overloads)
- Created `CascadedShadowMap` with GL_TEXTURE_2D_ARRAY (GL_DEPTH_COMPONENT32F), GL_COMPARE_REF_TO_TEXTURE, `glFramebufferTexture` for all-layer attachment
- Implemented `computeCascades()` with fixed splits (5m, 20m, farPlane) using `getFrustumCornersWorldSpace` and `buildCascadeMatrix` from RESEARCH.md
- `buildCascadeMatrix` uses zMult=10 to capture shadow casters outside frustum (RESEARCH critical pitfall)
- CSM depth shaders: geometry shader uses `layout(triangles, invocations = 3)` with `gl_Layer = gl_InvocationID` for single-pass multi-layer rendering
- Wired CSM into `RuntimeSceneRenderer`: init creates CSM, `renderShadowPass` computes cascades and renders to CSM array, `renderScenePass` binds CSM texture (unit 16) and sets uniforms
- Added `sampler2DArrayShadow uCsmShadowMap` to scene.frag with `selectCascade()`, `sampleCsmShadow()` — 16-tap Poisson PCF matching spot shadow quality
- `sampleCsmShadow` includes per-cascade bias scaling (bias / splitDistance) and 10% blend zone between adjacent cascades

## Task Commits

1. **Task 1: CascadedShadowMap class and CSM depth shaders** - `9a56bfb` (feat)
2. **Task 2: Wire CSM into shadow pass and scene.frag sampling** - `0ca1b4f` (feat)

## Files Created/Modified

- `src/engine/rendering/lighting/CascadedShadowMap.h` — CSM class: kCascadeCount=3, kDefaultResolution=1024, lightSpaceMatrices/splitDistances accessors
- `src/engine/rendering/lighting/CascadedShadowMap.cpp` — GL_TEXTURE_2D_ARRAY creation, glFramebufferTexture attachment, computeCascades, frustum corners extraction, cascade matrix building
- `assets/shaders/engine/csm_depth.vert` — Outputs world-space position (model transform only, no projection)
- `assets/shaders/engine/csm_depth.geom` — invocations=3, gl_Layer=gl_InvocationID, per-cascade light-space transform
- `assets/shaders/engine/csm_depth.frag` — Empty (depth written by rasterizer)
- `src/engine/rendering/core/Shader.h` — Added 3-arg constructor, Shader::load(vert,geom,frag) factory
- `src/engine/rendering/core/Shader.cpp` — Geometry shader constructor: compile GL_GEOMETRY_SHADER, attach, link
- `src/game/rendering/RuntimeSceneRenderer.h` — Added csmShader_ and csmShadowMap_ members, CascadedShadowMap include
- `src/game/rendering/RuntimeSceneRenderer.cpp` — CSM init, renderShadowPass receives CameraState, CSM depth pass after spot shadows, CSM uniforms set in renderScenePass
- `assets/shaders/game/scene.frag` — Added CSM uniforms, uViewMatrix, selectCascade(), sampleCsmShadow() with PCF+blending, applied to LIGHT_DIRECTIONAL in lighting loop
- `src/engine/CMakeLists.txt` — Added CascadedShadowMap.cpp to engine_rendering target

## Decisions Made

- Used `glFramebufferTexture` (not `glFramebufferTexture2D`) to attach all cascade layers to the FBO at once, enabling the geometry shader `gl_Layer` approach per RESEARCH.md
- Fixed cascade splits (5m, 20m, far) rather than logarithmic splitting — cleaner for indoor institutional scenes with short view distances
- CSM applied to all LIGHT_DIRECTIONAL types in scene.frag; `uCsmEnabled=0` causes sampleCsmShadow to return 1.0, so fill directional (typically disabled) receives no shadowing
- Geometry shader invocations approach preferred over N-pass rendering for single draw call efficiency

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED

- `src/engine/rendering/lighting/CascadedShadowMap.h` — FOUND
- `src/engine/rendering/lighting/CascadedShadowMap.cpp` — FOUND
- `assets/shaders/engine/csm_depth.geom` — FOUND (contains gl_InvocationID, invocations = 3)
- `assets/shaders/game/scene.frag` — FOUND (contains uCsmShadowMap, selectCascade, sampleCsmShadow)
- Commit `9a56bfb` — Task 1
- Commit `0ca1b4f` — Task 2
- Build: pixel-roguelike and level-editor compile cleanly

---
*Phase: 04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting*
*Completed: 2026-03-30*
