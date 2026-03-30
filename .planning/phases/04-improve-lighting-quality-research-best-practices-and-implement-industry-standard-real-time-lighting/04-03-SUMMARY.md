---
phase: 04-improve-lighting-quality
plan: 03
subsystem: rendering
tags: [opengl, glsl, ssao, ambient-occlusion, post-process, framebuffer, mrt, engine]

requires:
  - phase: 04-improve-lighting-quality
    plan: 02
    provides: CompositePass with bloomTex parameter, PostProcessParams, Framebuffer with normal texture

provides:
  - SsaoPass class with 32-sample hemisphere SSAO and 4x4 blur
  - Framebuffer extended to 3 MRT attachments (color, normals, geomNormals)
  - SSAO integrated in composite pipeline before bloom and tonemapping
  - AO parameters per-environment and via ImGui debug panel
  - geometry normal output in scene.frag (layout location 2)

affects:
  - 04-04-PLAN (shadow quality - shares Framebuffer)
  - 04-05-PLAN (area lights / final pass)

tech-stack:
  added: []
  patterns:
    - "SsaoPass follows BloomPass pattern: init/resize/render/aoTexture() accessors"
    - "Deterministic RNG seeds (42 for kernel, 123 for noise) for temporal stability"
    - "SSAO reads geometry normals (location 2), NOT normal-mapped normals, for artifact-free AO"
    - "AO applied as color *= ao BEFORE bloom and tonemapping in composite.frag"

key-files:
  created:
    - src/engine/rendering/post/SsaoPass.h
    - src/engine/rendering/post/SsaoPass.cpp
    - assets/shaders/engine/ssao.vert
    - assets/shaders/engine/ssao.frag
    - assets/shaders/engine/ssao_blur.vert
    - assets/shaders/engine/ssao_blur.frag
  modified:
    - assets/shaders/game/scene.frag
    - src/engine/rendering/core/Framebuffer.h
    - src/engine/rendering/core/Framebuffer.cpp
    - src/engine/rendering/post/PostProcessParams.h
    - src/engine/rendering/post/CompositePass.h
    - src/engine/rendering/post/CompositePass.cpp
    - assets/shaders/engine/composite.frag
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/game/rendering/EnvironmentDefinition.cpp
    - assets/defs/environments/default.environment
    - src/engine/ui/ImGuiLayer.cpp
    - src/editor/ui/EditorEnvironmentPanel.cpp
    - src/engine/CMakeLists.txt
    - apps/level_editor/main.cpp

key-decisions:
  - "Geometry normals (vNormal, unperturbed) written to layout location 2 so SSAO is not affected by per-material normal mapping"
  - "Fixed RNG seeds (42/123) ensure deterministic kernel and noise across frames — eliminates temporal shimmer"
  - "AO multiplied before bloom: contact shadows darken correctly before highlights are added"
  - "SsaoPass FBOs use GL_R8 (single channel) since AO is a scalar 0..1 factor"

patterns-established:
  - "Framebuffer now has 3 color attachments: 0=color, 1=normals (normal-mapped), 2=geomNormals (geometry-only)"
  - "SSAO wired with enableSsao guard: if disabled, ssaoPass_.render() is skipped and 0 is passed to composite"

requirements-completed: []

duration: ~30min
completed: 2026-03-30
---

# Phase 04 Plan 03: SSAO Summary

**Classic 32-sample hemisphere SSAO with 4x4 blur providing subtle contact darkening, wired into composite pipeline before bloom/tonemapping via geometry normal MRT output**

## Performance

- **Duration:** ~30 min
- **Started:** 2026-03-30T00:00:00Z
- **Completed:** 2026-03-30T00:30:00Z
- **Tasks:** 2
- **Files modified:** 15

## Accomplishments

- Extended Framebuffer to 3 MRT color attachments — geometry normals written at layout location 2 in scene.frag for use by SSAO (free of normal mapping artifacts)
- Implemented SsaoPass with 32-sample hemisphere kernel (deterministic seed 42), 4x4 noise texture (deterministic seed 123), and separate blur FBO pass
- Wired SSAO into RuntimeSceneRenderer pipeline and composite.frag with AO multiplied before bloom and tonemapping
- Added SSAO parameters (enabled, radius, bias, strength) to PostProcessParams, environment definition parser/serializer, default.environment, ImGuiLayer debug panel, and EditorEnvironmentPanel

## Task Commits

1. **Task 1: SsaoPass class, shaders, geometry normal output** - `ffecd89` (feat)
2. **Task 2: Wire SSAO into rendering pipeline** - `772df83` (feat)

## Files Created/Modified

- `src/engine/rendering/post/SsaoPass.h` — SSAO pass class declaration
- `src/engine/rendering/post/SsaoPass.cpp` — 32-sample SSAO with blur, deterministic kernel/noise
- `assets/shaders/engine/ssao.vert` — Fullscreen quad vertex shader for SSAO pass
- `assets/shaders/engine/ssao.frag` — Hemisphere SSAO with depth reconstruction and TBN sampling
- `assets/shaders/engine/ssao_blur.vert` — Fullscreen quad vertex shader for blur pass
- `assets/shaders/engine/ssao_blur.frag` — 4x4 box blur (16 samples)
- `assets/shaders/game/scene.frag` — Added fragGeomNormal output at layout location 2
- `src/engine/rendering/core/Framebuffer.h/.cpp` — Extended to 3 MRT attachments with geomNormalTex_
- `src/engine/rendering/post/PostProcessParams.h` — Added enableSsao, ssaoRadius, ssaoBias, ssaoStrength
- `src/engine/rendering/post/CompositePass.h/.cpp` — Added ssaoTex parameter, binds to texture unit 9
- `assets/shaders/engine/composite.frag` — Added uSsaoTex, uSsaoEnabled; AO multiply before bloom
- `src/game/rendering/RuntimeSceneRenderer.h/.cpp` — SsaoPass member, init/resize/render wiring
- `src/game/rendering/EnvironmentDefinition.cpp` — Parse and serialize ssao_* keys
- `assets/defs/environments/default.environment` — Added ssao_enabled true, ssao_radius 0.5, ssao_bias 0.025, ssao_strength 0.5
- `src/engine/ui/ImGuiLayer.cpp` — Added Ambient Occlusion collapsing header with 4 controls
- `src/editor/ui/EditorEnvironmentPanel.cpp` — Added SSAO DragFloat controls after bloom
- `src/engine/CMakeLists.txt` — Added SsaoPass.cpp to engine_rendering target
- `apps/level_editor/main.cpp` — Fixed compositePass.apply() call signature; fixed pre-existing kMaxShadowedSpotLightsLocal mismatch

## Decisions Made

- Geometry normals used for SSAO (not normal-mapped normals) per research Pitfall 1 — avoids material-dependent AO artifacts
- Fixed RNG seeds ensure the same kernel and noise every run, eliminating temporal shimmer
- AO applied before bloom so the darkening of contact shadows is preserved, not washed out by added highlights

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed level-editor CompositePass call with wrong argument count**
- **Found during:** Task 2 (wiring SSAO, updating CompositePass signature)
- **Issue:** `apps/level_editor/main.cpp` called `compositePass.apply()` with 7 args but updated signature requires 9 (added bloomTex in plan 02 was already breaking it; adding ssaoTex made it obvious)
- **Fix:** Added `0, 0` for bloomTex and ssaoTex — no bloom or SSAO in editor preview
- **Files modified:** `apps/level_editor/main.cpp`
- **Committed in:** `772df83` (Task 2 commit)

**2. [Rule 1 - Bug] Fixed pre-existing kMaxShadowedSpotLightsLocal type mismatch in level-editor**
- **Found during:** Task 2 (build check of level-editor after CompositePass fix)
- **Issue:** `kMaxShadowedSpotLightsLocal = 2` but `renderShadowPass()` expects `std::array<ShadowMap, kMaxShadowedSpotLights>` (size 8) — type mismatch preventing compilation
- **Fix:** Changed `kMaxShadowedSpotLightsLocal = kMaxShadowedSpotLights` (8) to match
- **Files modified:** `apps/level_editor/main.cpp`
- **Committed in:** `772df83` (Task 2 commit)

**3. [Rule 2 - Missing Critical] Added SSAO controls to EditorEnvironmentPanel**
- **Found during:** Task 2 (adding ImGui controls in ImGuiLayer)
- **Issue:** Plan only mentioned ImGuiLayer but the editor environment panel also exposes bloom/vignette controls — omitting SSAO there would create inconsistency and prevent save/load of AO settings from the editor
- **Fix:** Added 4 SSAO controls (checkbox + 3 drag floats) after bloom radius in EditorEnvironmentPanel
- **Files modified:** `src/editor/ui/EditorEnvironmentPanel.cpp`
- **Committed in:** `772df83` (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (2 Rule 1 bugs, 1 Rule 2 missing critical)
**Impact on plan:** All auto-fixes required for compilation and editor parity. No scope creep.

## Issues Encountered

None beyond the pre-existing level-editor build failures.

## Next Phase Readiness

- SSAO infrastructure is complete and compiles cleanly
- Framebuffer now has geometry normal attachment ready for future passes
- Plan 04 (shadow quality improvements) can proceed — shares the same Framebuffer

---
*Phase: 04-improve-lighting-quality*
*Completed: 2026-03-30*
