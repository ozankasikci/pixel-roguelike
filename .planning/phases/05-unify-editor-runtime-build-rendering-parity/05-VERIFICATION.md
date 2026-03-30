---
phase: 05-unify-editor-runtime-build-rendering-parity
verified: 2026-03-30T17:30:00Z
status: passed
score: 8/8 must-haves verified
re_verification: false
---

# Phase 5: Unify Editor/Runtime/Build Rendering Parity Verification Report

**Phase Goal:** Extract a shared SceneRenderPipeline from RuntimeSceneRenderer so the editor viewport, runtime game, and model viewer all render identically through the same code path — including bloom, SSAO, cascaded shadow maps, LTC area lights, tube lights, and emissive materials. The editor edit-mode viewport currently bypasses all Phase 4 rendering features. After this phase, what you see in the editor is what you get in the game.
**Verified:** 2026-03-30T17:30:00Z
**Status:** passed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1 | SceneRenderPipeline exists in engine_rendering with full pipeline (shadow, CSM, scene, bloom, SSAO, composite, stylize) | VERIFIED | `src/engine/rendering/SceneRenderPipeline.h` + `.cpp` (339 lines); contains BloomPass, SsaoPass, CompositePass, StylizePass, CascadedShadowMap, LtcData, ShadowMaps all as private members |
| 2 | SceneRenderPipeline.h has zero game-layer includes | VERIFIED | Grep of `game/` in header returns 0 matches; all 14 includes are engine-layer only |
| 3 | RuntimeSceneRenderer delegates all pipeline work to SceneRenderPipeline (thin ECS adapter) | VERIFIED | RSR.h contains only `SceneRenderPipeline pipeline_` — no BloomPass, SsaoPass, CascadedShadowMap etc. as direct members; RSR.cpp calls `pipeline_.init()`, `pipeline_.render(input,...)` |
| 4 | Editor edit-mode viewport renders with full Phase 4 features through SceneRenderPipeline | VERIFIED | `EditorViewportRenderer` exists (`.h` + `.cpp`), composes `SceneRenderPipeline pipeline_`, wired into `apps/level_editor/main.cpp` at line 292–293 and 1156; old CSM stub (`uCsmEnabled=0`) and old bloom/SSAO null comments removed from main.cpp |
| 5 | Model viewer uses SceneRenderPipeline instead of inline rendering | VERIFIED | `apps/model_viewer/main.cpp` line 157: `SceneRenderPipeline pipeline; pipeline.init();`, line 310: `pipeline.render(input, ...)`; old `compositePass.apply()`, `stylizePass.apply()`, `renderer.drawScene()` calls absent |
| 6 | Asset previews bind real LTC textures for area light support | VERIFIED | `EditorAssetPreviewRenderer.h` line 59: `LtcData ltcData_`; `.cpp` line 78: `ltcData_.init()`, line 203: `ltcData_.ltcMatTexture()`, line 206: `ltcData_.ltcAmpTexture()`; CSM disabled via `uCsmEnabled=0` |
| 7 | SceneRenderPipeline reports per-effect timing stats | VERIFIED | `SceneRenderPipelineStats` struct in header (lines 24–34); instrumented in `.cpp` with `glfwGetTime()` for totalRenderMs, shadowPassMs, scenePassMs, bloomMs, ssaoMs, compositeMs; `lastStats()` accessor at line 91 |
| 8 | All three executables compile | VERIFIED | `cmake --build .` succeeds; executables confirmed at `build/apps/runtime/pixel-roguelike`, `build/apps/level_editor/level-editor`, `build/apps/model_viewer/procedural-model-viewer` |

**Score:** 8/8 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/rendering/SceneRenderPipeline.h` | Shared render pipeline orchestrator; exports `SceneRenderPipeline`, `SceneRenderInput` | VERIFIED | 126 lines; contains both structs, full private member set, all public methods |
| `src/engine/rendering/SceneRenderPipeline.cpp` | Pipeline implementation; min 200 lines | VERIFIED | 339 lines; full shadow, CSM, scene, post-process chain |
| `src/engine/CMakeLists.txt` | Contains `SceneRenderPipeline.cpp` | VERIFIED | Line 30: `rendering/SceneRenderPipeline.cpp` |
| `src/game/rendering/RuntimeSceneRenderer.cpp` | Contains `pipeline_.render` | VERIFIED | Line 359: `pipeline_.render(input, internalWidth, internalHeight, outputWidth, outputHeight, targetFramebuffer)` |
| `src/editor/render/EditorViewportRenderer.h` | Editor viewport renderer class | VERIFIED | Contains `class EditorViewportRenderer`, `SceneRenderPipeline pipeline_` member |
| `src/editor/render/EditorViewportRenderer.cpp` | Editor viewport rendering implementation; min 100 lines | VERIFIED | 47 lines — below 100 line minimum from plan, but the plan itself revised this expectation (no MaterialTextureLibrary, so the class is intentionally thin); `pipeline_.render(input,...)` present |
| `apps/level_editor/main.cpp` | Simplified editor main using EditorViewportRenderer | VERIFIED | `editorViewportRenderer.render(renderParams, targetW, targetH, finalFbo.framebuffer())` at line 1156; old inline stubs absent |
| `apps/model_viewer/main.cpp` | Model viewer using SceneRenderPipeline | VERIFIED | Lines 157–158: `SceneRenderPipeline pipeline; pipeline.init();`; line 310: `pipeline.render(input,...)` |
| `src/editor/render/EditorAssetPreviewRenderer.h` | Contains `LtcData ltcData_` | VERIFIED | Line 59: `LtcData ltcData_;` |
| `src/editor/render/EditorAssetPreviewRenderer.cpp` | Contains `ltcData_.init()`, `ltcData_.ltcMatTexture()`, `uCsmEnabled` | VERIFIED | All three present at lines 78, 203, 211 |
| `src/engine/rendering/post/PostProcessParams.h` | Contains `float csmLambda` field | VERIFIED | Line 34: `float csmLambda = 0.5f;` |
| `src/editor/ui/EditorEnvironmentPanel.cpp` | Contains CSM Split Blend slider | VERIFIED | Line 199: `ImGui::DragFloat("CSM Split Blend", &environment.post.csmLambda, ...)` |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `RuntimeSceneRenderer.h` | `SceneRenderPipeline.h` | `SceneRenderPipeline pipeline_` member | WIRED | Line 62: `SceneRenderPipeline pipeline_` in header |
| `RuntimeSceneRenderer.cpp` | `pipeline_.render` | delegation call | WIRED | Line 359: `pipeline_.render(input, ...)` |
| `SceneRenderPipeline.cpp` | `BloomPass` | `bloomPass_` member | WIRED | `bloomPass_` declared in header; `bloomPass_.render(...)` called in `renderPostProcess` |
| `SceneRenderPipeline.cpp` | `SsaoPass` | `ssaoPass_` member | WIRED | `ssaoPass_` declared; `ssaoPass_.render(...)` called conditionally |
| `SceneRenderPipeline.cpp` | `CascadedShadowMap` | `csmShadowMap_` member | WIRED | `csmShadowMap_.computeCascades(...)` with csmLambda at line 174 |
| `SceneRenderPipeline.cpp` | `LtcData` | `ltcData_.ltcMatTexture()` | WIRED | Line 230: real LTC texture bound; never null |
| `SceneRenderPipeline.cpp` | SSAO via `geomNormalTexture()` | Pitfall 5 compliance | WIRED | Lines 289–291: `sceneFBO_.geomNormalTexture()` |
| `SceneRenderPipeline.cpp` | viewmodel depth trick | `glDepthRange(0.0, 0.01)` | WIRED | Lines 247–258: depth range applied for viewmodelObjects |
| `EditorViewportRenderer.cpp` | `SceneRenderPipeline` | `pipeline_.render` | WIRED | Line 46: `pipeline_.render(input, outputW, outputH, outputW, outputH, targetFBO)` |
| `apps/level_editor/main.cpp` | `EditorViewportRenderer` | `editorViewportRenderer.render(...)` | WIRED | Line 292–293: init; line 1156: render call |
| `apps/model_viewer/main.cpp` | `SceneRenderPipeline` | `pipeline.render(input,...)` | WIRED | Line 310: `pipeline.render(input, displayW, displayH, displayW, displayH)` |
| `EditorAssetPreviewRenderer.cpp` | `LtcData` | `ltcData_.init()` + bind before `drawScene` | WIRED | Lines 78, 203–211: init + bind LTC mat/amp + disable CSM |
| `PostProcessParams.csmLambda` | `CascadedShadowMap::computeCascades` | passed via `SceneRenderInput.postParams` | WIRED | SceneRenderPipeline.cpp line 174: `input.postParams ? input.postParams->csmLambda : 0.5f` |
| `csmLambda` | `EnvironmentDefinition` serializer | `csm_lambda` parse/write | WIRED | EnvironmentDefinition.cpp lines 536, 580 |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `SceneRenderPipeline::render()` | `lights`, `objects` from `SceneRenderInput` | Caller-supplied (RSR ECS collection or EditorViewportRenderer) | Yes — ECS registry queries in RSR; pre-collected by editor main.cpp | FLOWING |
| `SceneRenderPipeline::renderShadowPass()` | `shadowData.matrices`, `shadowData.textures` | Shadow maps from `shadowMaps_[i].texture()` | Yes — pre-populated for all 8 slots at lines 122–124 | FLOWING |
| `SceneRenderPipeline::renderPostProcess()` | bloom via `sceneFBO_.colorTexture()` | Scene FBO rendered in prior pass | Yes — rendered scene FBO | FLOWING |
| `SceneRenderPipeline::renderPostProcess()` | SSAO via `sceneFBO_.geomNormalTexture()` | Geometric normal attachment from scene FBO | Yes — uses correct geometric normal attachment (Pitfall 5 compliant) | FLOWING |
| `SceneRenderPipeline::renderPostProcess()` | LTC via `ltcData_.ltcMatTexture()` | LtcData initialized in `init()` | Yes — real LTC lookup tables, never null | FLOWING |
| `EditorViewportRenderer::render()` | `SceneRenderInput.postParams` | `params.environment->post` from EnvironmentDefinition | Yes — live environment state from editor document | FLOWING |
| `EditorAssetPreviewRenderer` | LTC textures bound to units 10/11 | `ltcData_` owned by renderer | Yes — `ltcData_.init()` called in `ensureInitialized()` | FLOWING |
| Stats overlay in `level-editor` | frame timing | `ImGui::GetIO().Framerate` (edit mode) / `runtimePreviewSession.performanceStats().lastRenderMs` (play mode) | Yes — ImGui measures actual frame time | FLOWING |

**Note on stats overlay:** The per-effect `SceneRenderPipelineStats` data (`shadowPassMs`, `bloomMs`, `ssaoMs`) is computed and stored in `pipeline_.lastStats_` after each render call, but `EditorViewportRenderer` does not expose a `lastStats()` accessor, so the editor viewport overlay cannot show the per-effect breakdown. The overlay shows generic frame ms/FPS which is correct and functional. This is a minor incompleteness: the `SceneRenderPipelineStats` struct and data exist but are not surfaced in the editor UI. This does not block any phase goal truth.

---

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| All three executables compile | `cmake --build .` | Succeeded; all targets built | PASS |
| `SceneRenderPipeline.h` has no game-layer includes | `grep "game/" SceneRenderPipeline.h` | 0 matches | PASS |
| RuntimeSceneRenderer.h has no pipeline members (thin adapter) | Grep for BloomPass/SsaoPass/etc. in header | 0 matches | PASS |
| Old editor CSM stub removed | Grep for `uCsmEnabled.*0` stub pattern in main.cpp | Not found | PASS |
| Old model viewer inline rendering removed | Grep for `compositePass.apply(`, `stylizePass.apply(`, `renderer.drawScene(` in model_viewer/main.cpp | 0 matches | PASS |
| `shadowData.matrices.fill(glm::mat4(1.0f))` used (not brace-init) | Line 121 SceneRenderPipeline.cpp | Present | PASS |
| `geomNormalTexture()` used for SSAO (not `normalTexture()`) | Lines 289–291 SceneRenderPipeline.cpp | Present | PASS |
| `glDepthRange` stays inside pipeline (viewmodel trick) | Lines 247–258 SceneRenderPipeline.cpp | Present | PASS |
| `csmLambda` flows from UI to computeCascades | PostProcessParams -> SceneRenderInput -> SceneRenderPipeline.cpp line 174 | Fully traced | PASS |
| LTC textures are real (not null) in asset preview | `ltcData_.ltcMatTexture()` at line 203 EditorAssetPreviewRenderer.cpp | Present | PASS |
| Commit hashes verified | git log for 9d841b3, da6f790, b08ca05, 33db9e5, 3ea03f3, df37b77 | All 6 commits found in git history | PASS |

---

### Requirements Coverage

Phase 5 has no REQUIREMENTS.md IDs (rendering architecture phase; explicitly not mapped). No orphaned requirements found — `grep -E "Phase 5" .planning/REQUIREMENTS.md` would return nothing.

---

### Anti-Patterns Found

No blocking or warning anti-patterns found in any of the key phase files. No TODO/FIXME/placeholder comments detected. No empty implementations. No stub patterns in rendering paths.

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `apps/level_editor/main.cpp` | 1232 | Stats overlay uses generic `ImGui::GetIO().Framerate` for edit-mode (not `pipeline_.lastStats()`) | Info | Per-effect breakdown (shadow ms, bloom ms) not shown in editor viewport stats. SceneRenderPipelineStats data is computed but not surfaced. Does not affect rendering correctness or phase goal. |

---

### Human Verification Required

The following items require running the application and cannot be verified programmatically:

#### 1. Visual Parity: Editor Edit-Mode vs Runtime

**Test:** Open level editor, load `assets/scenes/silos_cloister.scene`. Compare the edit-mode viewport with running `pixel-roguelike --scene assets/scenes/silos_cloister.scene`. Look for bloom glow on bright surfaces, SSAO contact shadows in corners, directional CSM shadows.
**Expected:** Both viewports show identical lighting quality — bloom, SSAO darkening in corners, CSM soft shadows on floor from directional light.
**Why human:** Visual comparison cannot be automated without screenshot diffing infrastructure.

#### 2. Live Environment Panel Response

**Test:** In the editor edit-mode viewport, drag the "Bloom Intensity" slider and "SSAO Strength" slider while watching the viewport.
**Expected:** Changes reflect immediately in the viewport (no reload required) — bloom intensity changes visible glow, SSAO strength changes contact shadow depth.
**Why human:** Requires interactive testing of real-time feedback loop.

#### 3. CSM Split Blend Slider Effect

**Test:** In the editor environment panel, drag the "CSM Split Blend" slider from 0.0 to 1.0 while looking at directional shadows.
**Expected:** Cascade distribution visibly shifts (0=linear, 1=logarithmic); shadow quality at distance changes.
**Why human:** Subtle visual effect requiring subjective assessment.

#### 4. Model Viewer Full Pipeline Features

**Test:** Run `procedural-model-viewer`, press TAB to enable stylized mode.
**Expected:** Post-process effects visible (bloom on bright materials, stylize pass edge detection, tone mapping). No GL_INVALID_OPERATION errors in console.
**Why human:** Requires running the application with an OpenGL context.

---

### Gaps Summary

No gaps. All 8 must-have truths are verified. All artifacts exist and are substantive. All key data flows are wired. The one informational note (per-effect stats not exposed in editor viewport overlay) does not block any phase goal — the overlay exists and shows timing, `SceneRenderPipelineStats` is computed, and the phase goal is rendering parity, not stats display granularity.

---

_Verified: 2026-03-30T17:30:00Z_
_Verifier: Claude (gsd-verifier)_
