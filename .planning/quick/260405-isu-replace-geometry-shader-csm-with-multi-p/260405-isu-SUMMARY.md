---
phase: quick
plan: 260405-isu
subsystem: engine/rendering/shadows
tags: [csm, shadow-map, geometry-shader, performance, opengl]
dependency_graph:
  requires: []
  provides: [multi-pass-csm]
  affects: [SceneRenderPipeline, CascadedShadowMap]
tech_stack:
  added: []
  patterns: [multi-pass-shadow-rendering, per-layer-fbo-attachment, front-face-culling]
key_files:
  modified:
    - src/engine/rendering/SceneRenderPipeline.cpp
    - src/engine/rendering/SceneRenderPipeline.h
    - src/engine/rendering/lighting/CascadedShadowMap.cpp
  deleted:
    - assets/shaders/engine/csm_depth.vert
    - assets/shaders/engine/csm_depth.geom
    - assets/shaders/engine/csm_depth.frag
decisions:
  - "Reuse shadow_depth shader for CSM passes: uLightViewProjection already present, per-cascade matrix set per loop iteration"
  - "glFramebufferTextureLayer in CascadedShadowMap::create() instead of glFramebufferTexture — FBO valid immediately for multi-pass"
  - "Remove glClear from bind(): multi-pass pipeline clears each layer individually in the cascade loop"
metrics:
  duration: ~5 minutes
  completed: "2026-04-05T10:36:47Z"
  tasks_completed: 2
  files_changed: 6
---

# Quick 260405-isu: Replace Geometry Shader CSM with Multi-Pass Rendering Summary

**One-liner:** Replaced geometry-shader-based CSM with three-pass rendering using `glFramebufferTextureLayer`, reusing the existing `shadow_depth` shader, and added front-face culling to the shadow pass.

## Objective

Eliminate the geometry shader from the CSM shadow pipeline. On macOS with Apple Silicon, geometry shaders are emulated via compute shaders, causing significant overhead. The multi-pass approach runs each cascade as a separate draw call targeting individual depth array layers, which is cheaper than a single geometry-shader pass that fans out to all layers.

## Tasks Completed

### Task 1: Replace geometry shader CSM with multi-pass and add front-face culling
**Commit:** `b87ee0f`

- Removed `csmShader_` member from `SceneRenderPipeline` and its construction in `init()`
- Replaced single geometry-shader CSM block with a `for (cascade)` loop:
  - `glFramebufferTextureLayer()` targets one depth array layer per iteration
  - `glViewport` + `glClear(GL_DEPTH_BUFFER_BIT)` per cascade
  - Per-cascade frustum culling — objects only in cascade 0 skip cascades 1 and 2
  - `shadowShader_->setMat4("uLightViewProjection", csmMatrices[cascade])` per pass
- Added `glCullFace(GL_FRONT)` / `glEnable(GL_CULL_FACE)` during the shadow pass and restored `GL_BACK` after, eliminating back-face shadow artifacts and halving rasterized fragment count

### Task 2: Delete geometry shader files and clean up CascadedShadowMap
**Commit:** `69a75c3`

- Deleted `assets/shaders/engine/csm_depth.{vert,geom,frag}` — geometry shader pipeline entirely removed
- `CascadedShadowMap::create()`: changed initial FBO attachment from `glFramebufferTexture` (full array) to `glFramebufferTextureLayer(..., 0)` (layer 0 only) so the FBO is immediately valid for multi-pass use
- `CascadedShadowMap::bind()`: removed `glClear(GL_DEPTH_BUFFER_BIT)` since the pipeline now clears each layer individually inside the cascade loop

## Deviations from Plan

None — plan executed exactly as written.

## Verification

- Build succeeded for both `level-editor` and `pixel-roguelike` targets after each task
- `csm_depth.*` shader files confirmed absent after Task 2
- `glFramebufferTextureLayer` used for both initial FBO setup and per-cascade attachment in the pipeline

## Self-Check: PASSED

- `src/engine/rendering/SceneRenderPipeline.cpp` — modified, compiled, linked
- `src/engine/rendering/SceneRenderPipeline.h` — modified, compiled, linked
- `src/engine/rendering/lighting/CascadedShadowMap.cpp` — modified, compiled, linked
- `assets/shaders/engine/csm_depth.vert` — deleted (confirmed absent)
- `assets/shaders/engine/csm_depth.geom` — deleted (confirmed absent)
- `assets/shaders/engine/csm_depth.frag` — deleted (confirmed absent)
- Commit `b87ee0f` — exists
- Commit `69a75c3` — exists
