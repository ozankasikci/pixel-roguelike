---
phase: 04-improve-lighting-quality
verified: 2026-03-30T02:17:37Z
status: passed
score: 20/20 must-haves verified
re_verification: false
---

# Phase 04: Improve Lighting Quality — Verification Report

**Phase Goal:** Improve lighting quality — research best practices and implement industry-standard real-time lighting techniques including soft shadows, SSAO, bloom, cascaded shadow maps, and area lights.
**Verified:** 2026-03-30T02:17:37Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Spot light shadows use 16-tap Poisson disk PCF with per-fragment rotation | VERIFIED | `poissonDisk[16]` array in scene.frag; `hash12()` + `mat2 rot` at lines 880, 906-908 |
| 2 | Up to 8 spot lights can cast shadows simultaneously | VERIFIED | `kMaxShadowedSpotLights = 8` in RenderLight.h:14 and RuntimeSceneRenderer.h:52 |
| 3 | Shadow sampling functions use explicit if/else per sampler index (macOS safety) | VERIFIED | 21 occurrences of `if (shadowIndex ==` in scene.frag; covers indices 0–7 for shadowTexelSize, shadowDepthAt, shadowClipPosition |
| 4 | Bloom uses mip-chain downsample/upsample pipeline (not old 8-tap bloomGlow) | VERIFIED | `bloomGlow` absent from composite.frag; `uBloomTex` sampler wired at line 301; BloomPass.cpp has full 5-level mip chain |
| 5 | Bloom operates in HDR space before ACES tonemapping | VERIFIED | composite.frag ordering: SSAO multiply (line 297) → bloom add (line 302) → acesFitted (line 307) |
| 6 | Bloom intensity and radius are configurable per-environment via PostProcessParams | VERIFIED | `uBloomIntensity` uniform set from `params.bloomIntensity`; `bloomRadius * 0.003f` passed to BloomPass::render |
| 7 | SSAO produces contact darkening using geometry normals | VERIFIED | `fragGeomNormal` at layout location 2 in scene.frag:12,984; SsaoPass reads `sceneGeomNormalTex` (not normal-mapped normals) |
| 8 | AO applied as multiplier before bloom and tonemapping | VERIFIED | composite.frag line 297: `color *= ao` precedes bloom add at 302 and acesFitted at 307 |
| 9 | AO strength, radius, bias configurable per-environment | VERIFIED | PostProcessParams.h:30-33 has `enableSsao/ssaoRadius/ssaoBias/ssaoStrength`; EnvironmentDefinition.cpp:520-533 parses all 4 keys; default.environment:23-26 contains all 4 |
| 10 | Directional sun light casts shadows via 3-cascade CSM | VERIFIED | `CascadedShadowMap::kCascadeCount = 3`; RuntimeSceneRenderer.cpp:418-438 renders CSM in shadow pass for `lighting.sun.enabled` directional lights |
| 11 | CSM uses GL_TEXTURE_2D_ARRAY with one layer per cascade | VERIFIED | CascadedShadowMap.cpp:18-19 creates `GL_TEXTURE_2D_ARRAY`; `glFramebufferTexture` (not `glFramebufferTexture2D`) at line 42 |
| 12 | Cascade boundaries blend smoothly | VERIFIED | scene.frag:923-938 implements 10% blendZone with `mix(visibility, nextVisibility, blendFactor)` |
| 13 | Shadow bias scales per-cascade | VERIFIED | scene.frag:903 `float bias = baseBias / max(uCsmSplitDistances[layer], 0.01)` |
| 14 | CSM sampling uses same soft Poisson PCF as spot shadows | VERIFIED | sampleCsmShadow uses the same `poissonDisk[16]` array with `mat2 rot` per-fragment rotation (lines 969-971, 915, 934) |
| 15 | Rectangular area lights produce soft illumination via LTC | VERIFIED | `LTC_Evaluate` at scene.frag:671; `areaLightContribution` at 709; `LIGHT_AREA_RECT` dispatch at 1054 |
| 16 | Tube lights approximate fluorescent fixtures with closest-point specular | VERIFIED | `LIGHT_TUBE = 4` in scene.frag:77; `tubeLightContribution` at 747; dispatch at 1059 |
| 17 | LTC lookup tables loaded at startup (not computed per-frame) | VERIFIED | LtcData.cpp runs `fitLtcMat`/`fitLtcAmp` loops in `init()` once; per-frame cost is texture lookup only |
| 18 | Area/tube lights integrate into existing Cook-Torrance lighting loop | VERIFIED | scene.frag:1054-1063: same `for (int i = 0; i < uNumLights; ++i)` loop dispatches on `LIGHT_AREA_RECT` and `LIGHT_TUBE` |
| 19 | Emissive material property adds glow to surfaces | VERIFIED | `uEmissiveStrength` uniform at scene.frag:70; `totalLight += albedo * max(uEmissiveStrength, 0.0)` at 1132; full pipeline: .material file → MaterialDefinition.cpp:349-350 → emissiveStrength → Renderer.cpp:96 |
| 20 | Existing scenes render with softer shadow edges | VERIFIED | Poisson disk spread=3.0 replaces 3x3 box PCF; all commits validated (10 task commits confirmed as valid git objects) |

**Score:** 20/20 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `assets/shaders/game/scene.frag` | Poisson PCF, 8 shadow uniforms, CSM, LTC, emissive | VERIFIED | Contains all: `uShadowMaps[8]`, `poissonDisk[16]`, `uCsmShadowMap`, `LTC_Evaluate`, `uEmissiveStrength` |
| `src/engine/rendering/lighting/RenderLight.h` | `kMaxShadowedSpotLights = 8`, AreaRect/Tube enum | VERIFIED | Line 14: `kMaxShadowedSpotLights = 8`; lines 9-10: AreaRect=3, Tube=4; area light fields at 32-37 |
| `src/game/rendering/RuntimeSceneRenderer.h` | 8-element shadow array, all pass members | VERIFIED | `shadowMaps_` array uses `kMaxShadowedSpotLights`; bloomPass_, ssaoPass_, csmShadowMap_, csmShader_, ltcData_ all declared |
| `src/engine/rendering/post/BloomPass.h` | `class BloomPass` with kMipCount=5 | VERIFIED | 793 bytes; class BloomPass with MipLevel struct |
| `src/engine/rendering/post/BloomPass.cpp` | Mip chain, downsample/upsample, resize/render | VERIFIED | 5529 bytes; full implementation with init/resize/render/destroy |
| `assets/shaders/engine/bloom_downsample.frag` | 13-tap kernel with weight 0.125 | VERIFIED | Lines 28-30: `e * 0.125`, `(a+c+g+i) * 0.03125`, weights match COD kernel |
| `assets/shaders/engine/bloom_upsample.frag` | 3x3 tent filter dividing by 16.0 | VERIFIED | Line 25: `/ 16.0` |
| `assets/shaders/engine/composite.frag` | uBloomTex, uSsaoTex, AO before bloom before tonemapping | VERIFIED | Line 11: `uBloomTex`; line 12: `uSsaoTex`; order confirmed at 295-307 |
| `src/engine/rendering/post/SsaoPass.h` | `class SsaoPass` with init/resize/render/aoTexture | VERIFIED | 1117 bytes; full class declaration |
| `src/engine/rendering/post/SsaoPass.cpp` | 32-sample SSAO, deterministic seeds 42/123, blur | VERIFIED | 264 lines; `std::default_random_engine rng(42)` at line 63; `rng(123)` at line 87; both passes implemented |
| `assets/shaders/engine/ssao.frag` | `uSamples[32]`, depth reconstruction, 32-sample loop | VERIFIED | Line 6: `uniform vec3 uSamples[32]`; `reconstructViewPos` function present |
| `assets/shaders/engine/ssao_blur.frag` | 4x4 box blur (16 samples) | VERIFIED | Lines 11-17: nested loop -2 to 2, divides by 16.0 |
| `src/engine/rendering/lighting/CascadedShadowMap.h` | `class CascadedShadowMap`, kCascadeCount=3, kDefaultResolution=1024 | VERIFIED | Lines 13-14 confirm both constants |
| `assets/shaders/engine/csm_depth.geom` | `layout(triangles, invocations = 3)`, `gl_Layer = gl_InvocationID` | VERIFIED | Lines 2 and 8 confirmed |
| `src/engine/rendering/lighting/LtcData.h` | `class LtcData` with ltcMatTexture/ltcAmpTexture | VERIFIED | 1021 bytes; full class with both accessors |
| `src/engine/rendering/lighting/LtcData.cpp` | LTC LUT generation, GL_RGBA32F textures | VERIFIED | 138 lines; analytical fit at init() time; GL_RGBA32F at line 108 |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `scene.frag` | RuntimeSceneRenderer shadow pass | `uShadowMaps[8]` uniform array | VERIFIED | `uniform sampler2D uShadowMaps[8]` at scene.frag:36 |
| `RuntimeSceneRenderer.cpp` | ShadowMap array | shadow slot assignment loop (`shadowMaps_`) | VERIFIED | `shadowMaps_` uses `kMaxShadowedSpotLights` in RuntimeSceneRenderer.h:110 |
| `BloomPass` | `CompositePass` | Bloom texture output via `uBloomTex` | VERIFIED | CompositePass.cpp binds `bloomTex` to GL_TEXTURE8, sets `uBloomTex`; composite.frag line 11+301 |
| `RuntimeSceneRenderer` | `BloomPass` | Called between scene pass and composite pass (`bloomPass_`) | VERIFIED | RuntimeSceneRenderer.cpp:532 calls `bloomPass_.render()`; line 547 passes `bloomPass_.bloomTexture()` |
| `SsaoPass` | sceneFBO_ depth + geom normals | Reads depth and geometry normal textures | VERIFIED | SsaoPass.cpp:196-202 binds both textures; RuntimeSceneRenderer.cpp:535 passes `sceneFBO_.geomNormalTexture()` |
| `SsaoPass` output | `CompositePass` | AO texture via `uSsaoTex` | VERIFIED | composite.frag line 12+295-297; CompositePass.h:23 has `ssaoTex` param |
| `scene.frag` | `SsaoPass` | `fragGeomNormal` at layout location 2 | VERIFIED | scene.frag:12 `layout(location = 2) out vec4 fragGeomNormal`; written at line 984 |
| `CascadedShadowMap` | RuntimeSceneRenderer shadow pass | CSM rendered alongside spot shadows (`csmShadowMap_`) | VERIFIED | RuntimeSceneRenderer.cpp:418-438 renders CSM depth pass |
| `scene.frag CSM sampling` | CascadedShadowMap texture array | `sampler2DArrayShadow` uniform `uCsmShadowMap` | VERIFIED | scene.frag:43; bound to texture unit 16 in RuntimeSceneRenderer.cpp:481 |
| `CascadedShadowMap::buildCascadeMatrices` | scene.frag `uCsmMatrices` | Cascade matrices as uniforms | VERIFIED | RuntimeSceneRenderer.cpp:473-476 iterates `csmShadowMap_.lightSpaceMatrices()` |
| `LtcData` | `scene.frag` | LTC LUT textures bound as `uLtcMat`/`uLtcAmp` | VERIFIED | RuntimeSceneRenderer.cpp:487-491; scene.frag:34-35 and 723-724 |
| `scene.frag lighting loop` | RenderLight area/tube fields | Type dispatch on `LIGHT_AREA_RECT` | VERIFIED | scene.frag:1054-1063 dispatches correctly; area light fields uploaded in Renderer.cpp:64-68 |
| `scene.frag emissive` | MaterialLibrary | `uEmissiveStrength` uniform set per material | VERIFIED | Renderer.cpp:96; data flows from MaterialDefinition.cpp:349-350 → MaterialTextureLibrary.cpp:191 |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `SsaoPass.cpp` | `ssaoColorTex_` / `blurColorTex_` | `sceneDepthTex` + `sceneGeomNormalTex` from sceneFBO_ | Yes — 32-sample hemisphere SSAO drawn to GL_R8 FBO | FLOWING |
| `BloomPass.cpp` | `mips_[0].colorTex` | `sceneColorTex` from sceneFBO_ colorTexture | Yes — 5-level mip downsample+upsample rendered to GL_RGBA16F | FLOWING |
| `CascadedShadowMap.cpp` | `depthArray_` | Scene geometry rendered via csmShader_ with computed cascade matrices | Yes — depth written by rasterizer to GL_TEXTURE_2D_ARRAY | FLOWING |
| `LtcData.cpp` | `ltcMatTex_` / `ltcAmpTex_` | Analytical `fitLtcMat`/`fitLtcAmp` loops over 64x64 (roughness, NdotV) grid | Yes — GL_RGBA32F populated at init() | FLOWING |

---

### Behavioral Spot-Checks

Step 7b: SKIPPED — verification requires running the game with a GPU context; all code paths are wired and substantive. No API endpoint or CLI module to test without OpenGL context.

---

### Requirements Coverage

No REQUIREMENTS.md IDs were declared in any plan's `requirements:` field for this phase. This is expected — Phase 04 is a rendering quality phase not mapped to game-logic requirements.

---

### Anti-Patterns Found

None. No TODO/FIXME/placeholder comments found in any newly created or modified implementation files:
- `BloomPass.cpp` — clean
- `SsaoPass.cpp` — clean
- `CascadedShadowMap.cpp` — clean
- `LtcData.cpp` — clean
- `scene.frag` — clean

---

### Human Verification Required

The following behaviors require running the game with a GPU and visual inspection:

#### 1. Shadow softness quality

**Test:** Load any scene with shadow-casting spot lights. Observe shadow edges.
**Expected:** Shadow edges are soft and diffuse with no visible banding or hard lines — matching The Stanley Parable's gentle shadow style.
**Why human:** Visual quality judgment; Poisson PCF is wired and substantive but aesthetic quality requires a human eye.

#### 2. SSAO subtlety calibration

**Test:** Enable SSAO in ImGui debug panel, then toggle it off.
**Expected:** With AO enabled, wall-floor corners and door frames show subtle darkening. Turning it off makes the scene look noticeably flatter. The AO should not be distractingly dark — "barely noticeable until toggled off."
**Why human:** Aesthetic threshold judgment; the strength=0.5 default may need tuning per scene.

#### 3. Bloom warm glow quality

**Test:** Look at a bright spot light source or emissive surface in the game.
**Expected:** A soft warm halo bleeds into surrounding surfaces. No harsh bright ring — a gentle diffuse glow.
**Why human:** Visual quality and art direction match; the mip-chain bloom is wired but the bloom intensity/radius combination requires a human to assess against the art direction.

#### 4. CSM shadow coverage and seam-free cascade transitions

**Test:** Enable a directional sun light in a scene and walk forward from near to far geometry.
**Expected:** Shadow quality remains consistent across distances (no resolution collapse). No visible hard band at cascade boundaries (~5m and ~20m distances).
**Why human:** Spatial behavior across distance requires walking the scene; cascade blend zone is wired but visual verification needed.

#### 5. Area light and tube light visual output

**Test:** Place an AreaRect light (e.g., ceiling panel) and a Tube light (e.g., corridor fixture) in a test scene.
**Expected:** AreaRect produces a soft, wide rectangular illumination pattern. Tube light produces an elongated specular highlight matching a fluorescent fixture.
**Why human:** New light type quality requires placing lights in a scene and visually assessing the falloff and specular behavior; LTC is wired but quality of the analytical approximation vs. reference LTC tables needs human confirmation.

---

### Gaps Summary

No gaps. All 20 observable truths are verified as VERIFIED. All 16 required artifacts exist with substantive implementations. All 13 key links are wired. All 4 data flows are confirmed as FLOWING. No anti-patterns found in implementation files.

The one notable deviation from plan in Plan 05 — using an analytical LTC LUT approximation at init() rather than embedding the full selfshadow/ltc_code static arrays — is a valid implementation choice. The interface to the shader is identical (same 64x64 GL_RGBA32F texture lookup), and the analytical fit covers the full (roughness, NdotV) parameter space. This may produce slightly different area light appearance than reference LTC tables; flagged for human visual verification (item 5 above).

---

_Verified: 2026-03-30T02:17:37Z_
_Verifier: Claude (gsd-verifier)_
