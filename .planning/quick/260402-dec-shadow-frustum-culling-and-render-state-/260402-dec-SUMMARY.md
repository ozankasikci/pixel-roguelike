# Quick Task 260402-dec: Shadow frustum culling and render state sorting — Summary

**Completed:** 2026-04-02
**Status:** Done

## What Changed

Two performance optimizations in the rendering pipeline:

### 1. Shadow Frustum Culling
- **Spot light shadows:** Extract frustum planes from each light's VP matrix, cull objects outside the light's projection volume
- **CSM shadows:** Extract frustum planes from each of the 3 cascade matrices, skip objects that are outside ALL cascades (conservative — renders if in any cascade)
- Uses existing `extractFrustumPlanes()` / `isAabbInsideFrustum()` infrastructure
- Tracks `shadowCulledCount` in pipeline stats for monitoring

### 2. Render State Sorting
- Sort culled objects by material ID (grouping same materials), then front-to-back distance from camera for early-Z rejection
- `Renderer::drawScene` tracks `lastMaterialId` — skips ~30 `glUniform` calls and 4 `glBindTexture` calls for consecutive objects sharing the same material
- Per-object uniforms (model matrix, base color, unlit flag) always set

## Files Modified

| File | Change |
|------|--------|
| `src/engine/rendering/SceneRenderPipeline.h` | Added `shadowCulledCount` to stats struct |
| `src/engine/rendering/SceneRenderPipeline.cpp` | Shadow frustum culling in `renderShadowPass`, material sort in `render()` |
| `src/engine/rendering/geometry/Renderer.cpp` | Redundant state elimination with `lastMaterialId` tracking |
| `apps/level_editor/main.cpp` | Show `shadow_culled` in stats overlay |
