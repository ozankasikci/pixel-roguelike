# Phase 5: Unify Editor/Runtime/Build Rendering Parity — Research

**Researched:** 2026-03-30
**Domain:** OpenGL rendering pipeline extraction / C++ refactor — editor/runtime/model-viewer parity
**Confidence:** HIGH (all evidence comes from direct codebase reading)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Full parity — bring ALL Phase 4 features into the editor viewport: bloom (mip-chain), SSAO, CSM directional shadows, LTC area lights, tube lights, and emissive material support
- **D-02:** All light types render correctly in edit-mode — area lights (LTC), tube lights, emissive surfaces all show accurate lighting while placing fixtures
- **D-03:** Environment panel adjustments (exposure, bloom intensity, AO strength, fog, CSM cascade distances) reflect instantly in the viewport — live preview, not apply-on-save
- **D-04:** Keep existing debug view modes (Final, Lighting Only, Sky Only) as-is — no new debug views for individual post-process effects
- **D-05:** Environment panel exposes all new Phase 4 parameters (SSAO radius/intensity, bloom threshold/intensity, CSM cascade distances) for interactive tuning
- **D-06:** Extract a `SceneRenderPipeline` class in `src/engine/rendering/` that encapsulates the full rendering pipeline: scene pass, shadow pass (spot + CSM), SSAO, bloom, composite, stylize
- **D-07:** Refactor `RuntimeSceneRenderer` to compose `SceneRenderPipeline` — RSR becomes a thin wrapper adding game-specific logic (viewmodel rendering, runtime camera capture)
- **D-08:** Refactor shared logic (`collectLights`, `renderShadowPass`, shadow slot assignment) into the shared pipeline. Keep only editor-specific helpers (selection overlays, gizmos, collider wireframes) in editor code
- **D-09:** Extract `EditorViewportRenderer` class from the ~800 lines of inline rendering in `apps/level_editor/main.cpp`. This class composes `SceneRenderPipeline` + editor-specific overlays
- **D-10:** Include the model viewer (`procedural-model-viewer`) — all three executables use the same shared pipeline for identical output
- **D-11:** Editor overlay integration (gizmos, selection highlights, wireframes) — Claude's discretion on the cleanest integration point
- **D-12:** Asset previews (mesh/material thumbnails in EditorAssetPreviewRenderer) get simplified lighting: correct light types and emissive support, but no SSAO and no bloom
- **D-13:** Asset previews use a fixed neutral studio lighting environment, not the current scene's environment definition
- **D-14:** Per-effect toggles in the editor (bloom, SSAO, CSM, shadows individually toggleable). All enabled by default. DebugParams already has toggle fields — extend as needed
- **D-15:** Editor viewport renders at native resolution always — no render scale slider or downscaling
- **D-16:** Viewport overlay shows performance stats (frame time, draw calls, per-effect timing). Extend the existing RuntimeSessionPerformanceStats pattern to the editor

### Claude's Discretion

- SceneRenderPipeline class API design and initialization interface
- How editor overlays integrate with the shared pipeline (callback hook, render object flags, or separate pass after post-processing)
- FBO management strategy (shared pool vs per-consumer allocation)
- CMake target dependencies when moving rendering to engine layer
- Order of implementation (pipeline extraction first, then wire editor, then model viewer — or interleaved)
- Which shared functions to make methods vs free functions
- How to handle the neutral studio lighting environment for asset previews (embedded constants vs .environment file)

### Deferred Ideas (OUT OF SCOPE)

- Render scale slider for weaker hardware — native resolution only for now
- SSAO-only / Bloom-only / Shadow-cascades debug view modes — not needed this phase
- Asset preview full post-processing (SSAO + bloom at thumbnail size has diminishing returns)
- Auto-scaling quality based on frame time monitoring
- Quality presets (Low/Medium/High) for the editor
</user_constraints>

---

## Summary

Phase 5 extracts the full rendering pipeline from `RuntimeSceneRenderer` into a reusable `SceneRenderPipeline` class so that the editor viewport, the runtime game, and the procedural model viewer all produce identical visual output. The code analysis reveals that every component of the pipeline already exists as engine-layer objects (BloomPass, SsaoPass, CompositePass, StylizePass, CascadedShadowMap, LtcData) — what is missing is an orchestrator class that ties them together. RuntimeSceneRenderer already IS that orchestrator for the runtime game. The task is to elevate its core logic to the engine layer.

The editor viewport (apps/level_editor/main.cpp, ~1808 lines) currently stubs out all Phase 4 features at lines 1180-1215 with explicit null bindings for CSM (uCsmEnabled=0), LTC samplers (bound to 0 to prevent GL errors), and passes 0 for bloom and SSAO to CompositePass. The model viewer likewise omits CSM, LTC, SSAO, and bloom. The path forward is: (1) create `SceneRenderPipeline` in `src/engine/rendering/`, (2) refactor RSR to delegate to it, (3) extract `EditorViewportRenderer` class to own what is currently inline in main.cpp, (4) wire the model viewer.

The most important architectural insight from the code: `RuntimeSceneRenderer::render()` is a 30-line orchestration function that calls `collectSceneObjects`, `collectViewmodelObjects`, `collectLights`, `assignShadowSlots`, `renderScenePass`, `renderPostProcess`, and `updateDebugParams`. The parts that belong to the shared pipeline are: shadow pass, CSM pass, scene pass, bloom, SSAO, composite, stylize, and all shadow-slot/matrix bookkeeping. The parts that are RSR-only: ECS camera capture, viewmodel object collection, player torch light injection, `syncEnvironmentFromRegistry`.

**Primary recommendation:** Create `SceneRenderPipeline` in `engine_rendering` (it already links everything it needs). Make its public API accept `SceneRenderInput` (objects, lights, camera matrices, post params, debug toggles) and produce output to a target FBO. RSR and EditorViewportRenderer become thin callers that prepare their SceneRenderInput from their respective sources.

---

## Standard Stack

All libraries are already present in the project. No new dependencies.

### Core (already in engine_rendering)
| Class | Location | Purpose | Status |
|-------|----------|---------|--------|
| `BloomPass` | `src/engine/rendering/post/BloomPass.h` | Mip-chain bloom (5 half-res FBOs, 13-tap downsample, tent upsample) | Already engine-layer |
| `SsaoPass` | `src/engine/rendering/post/SsaoPass.h` | 32-sample hemisphere SSAO with blur | Already engine-layer |
| `CompositePass` | `src/engine/rendering/post/CompositePass.h` | Tonemapping, bloom compositing, AO application, sky | Already engine-layer |
| `StylizePass` | `src/engine/rendering/post/StylizePass.h` | Edge detection stylization | Already engine-layer |
| `CascadedShadowMap` | `src/engine/rendering/lighting/CascadedShadowMap.h` | 3-cascade CSM, GL_TEXTURE_2D_ARRAY, geometry shader | Already engine-layer |
| `LtcData` | `src/engine/rendering/lighting/LtcData.h` | LTC lookup tables for area lights (units 10/11) | Already engine-layer |
| `ShadowMap` | `src/engine/rendering/lighting/ShadowMap.h` | Per-spotlight shadow map | Already engine-layer |
| `Renderer` | `src/engine/rendering/geometry/Renderer.h` | Low-level draw call batching | Already engine-layer |
| `Framebuffer` | `src/engine/rendering/core/Framebuffer.h` | RAII FBO with resize | Already engine-layer |
| `Shader` | `src/engine/rendering/core/Shader.h` | GLSL shader wrapper | Already engine-layer |

### New Class to Create
| Class | Proposed Location | CMake Target | Purpose |
|-------|------------------|--------------|---------|
| `SceneRenderPipeline` | `src/engine/rendering/SceneRenderPipeline.h/.cpp` | `engine_rendering` | Full pipeline orchestrator |
| `EditorViewportRenderer` | `src/editor/render/EditorViewportRenderer.h/.cpp` | `editor` | Editor-specific pipeline consumer |

---

## Architecture Patterns

### Existing Pipeline Chain (RuntimeSceneRenderer — source of truth)

The full render chain in RSR:
```
collectLights (ECS) → assignShadowSlots → renderShadowPass (spot lights) → CSM pass → sceneFBO scene pass → bloomPass → ssaoPass → compositePass → stylizePass → targetFBO
```

`renderScenePass` binds CSM texture to unit 16, LTC mat/amp to units 10/11, then calls `renderer_->drawScene()`.

`renderPostProcess` calls:
1. `bloomPass_.render(sceneFBO_.colorTexture(), ...)`
2. `ssaoPass_.render(sceneFBO_.depthTexture(), sceneFBO_.geomNormalTexture(), ...)`
3. `compositePass_.apply(scene, depth, normal, bloom, ssao, compositeFBO, ...)`
4. `stylizePass_.apply(composite, scene, depth, normal, ..., targetFBO)`

### Recommended SceneRenderPipeline API Design

```cpp
// src/engine/rendering/SceneRenderPipeline.h
// Source: Direct codebase analysis

struct SceneRenderInput {
    // Objects and lights
    const std::vector<RenderObject>* objects = nullptr;
    const std::vector<RenderObject>* overlayObjects = nullptr;  // wireframes, gizmo helpers — rendered after post-process
    const std::vector<RenderLight>* lights = nullptr;

    // Camera
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::vec3 cameraPosition{0.0f};
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Post-process and debug params (pipeline reads toggles from here)
    const PostProcessParams* postParams = nullptr;
    bool shadowsEnabled = true;
    int shadowResolutionIndex = 1;
    float shadowBias = 0.0018f;
    float shadowNormalBias = 0.03f;
    LightingEnvironment lightingEnvironment;
};

class SceneRenderPipeline {
public:
    void init();
    void shutdown();
    void resize(int width, int height);

    // Render full pipeline to targetFBO (0 = default framebuffer)
    void render(const SceneRenderInput& input,
                int internalWidth, int internalHeight,
                int outputWidth, int outputHeight,
                GLuint targetFramebuffer = 0);

    Framebuffer& sceneFBO() { return sceneFBO_; }

private:
    static constexpr int kMaxShadowedSpotLights = 8;

    void ensureFramebuffers(int w, int h);
    void assignShadowSlots(std::vector<RenderLight>& lights, bool enabled) const;
    glm::mat4 buildShadowMatrix(const RenderLight& light) const;
    void renderShadowPass(const std::vector<RenderObject>& objects,
                          const std::vector<RenderLight>& lights,
                          const SceneRenderInput& input,
                          ShadowRenderData& shadowData);
    void renderScenePass(const SceneRenderInput& input,
                         int w, int h,
                         const ShadowRenderData& shadowData);
    void renderPostProcess(const SceneRenderInput& input,
                           int outputW, int outputH,
                           GLuint targetFBO);

    Framebuffer sceneFBO_;
    Framebuffer compositeFBO_;
    std::unique_ptr<Shader> sceneShader_;
    std::unique_ptr<Shader> shadowShader_;
    std::unique_ptr<Shader> csmShader_;
    std::unique_ptr<Renderer> renderer_;
    BloomPass bloomPass_;
    SsaoPass ssaoPass_;
    CompositePass compositePass_;
    StylizePass stylizePass_;
    LtcData ltcData_;
    std::array<ShadowMap, kMaxShadowedSpotLights> shadowMaps_{};
    CascadedShadowMap csmShadowMap_;
};
```

### SceneRenderPipeline Does NOT Own
- MaterialTextureLibrary — stays in RSR (game-layer) and EditorViewportRenderer (editor-layer)
- ECS camera/object collection — stays in RSR (runtime-specific) and editor viewport controller
- Player torch lights, viewmodel objects — RSR-specific
- Environment sync from registry — RSR-specific

### Refactored RuntimeSceneRenderer (after extraction)

RSR becomes a thin ECS adapter:
```cpp
void RuntimeSceneRenderer::render(...) {
    syncEnvironmentFromRegistry(...);
    CameraState camera = captureCamera(registry, aspect);
    auto objects = collectSceneObjects(registry);
    auto viewmodelObjects = collectViewmodelObjects(registry, camera, deltaTime);
    auto lights = collectLights(registry, params);  // includes player torch injection

    SceneRenderInput input;
    input.objects = &objects;
    // viewmodel objects passed separately since they use glDepthRange(0, 0.01)
    input.lights = &lights;
    input.viewMatrix = camera.viewMatrix;
    // ... fill remaining fields from camera + params ...

    pipeline_.render(input, internalWidth, internalHeight, outputWidth, outputHeight, targetFramebuffer);
    // render viewmodel pass (glDepthRange trick) separately using pipeline_.sceneFBO()
    updateDebugParams(params, camera, deltaTime, objects.size());
}
```

### EditorViewportRenderer (extracted from main.cpp)

```cpp
class EditorViewportRenderer {
public:
    void init(const ContentRegistry& content);
    void shutdown();
    void resize(int width, int height);
    void render(const EditorViewportRenderParams& params,
                int outputW, int outputH,
                GLuint targetFBO);

private:
    SceneRenderPipeline pipeline_;
    MaterialTextureLibrary materialTextures_;
    // Editor shaders stay here only if EditorViewportRenderer needs them
    // (e.g., for selection highlights rendered before scene pass via overlayObjects)
};
```

### Editor Overlay Integration (Claude's Discretion — Recommendation)

The cleanest integration: pass overlay objects (wireframes, collider outlines, selection boxes, gizmo helpers) as a separate `overlayObjects` list in `SceneRenderInput`. The pipeline renders them after the main scene pass but before post-processing — so they receive stylize pass treatment (edge detection sharpens them nicely). This matches how `appendHelperObjects` and `appendSelectionOverlays` currently work in `EditorScenePreviewRenderer.cpp` — they already push into the same `objects` vector, so the pattern is preserved.

Alternative: render overlays as a post-process overlay pass after `stylizePass_`. This would let wireframes bypass bloom/SSAO but they'd lose edge enhancement. Given that the current editor already mixes helper objects into the main draw, keep them in the same scene pass.

### FBO Management Strategy (Claude's Discretion — Recommendation)

Per-consumer allocation: `SceneRenderPipeline` owns its own `sceneFBO_`, `compositeFBO_`, and all bloom/SSAO internal FBOs. The RSR-owned pipeline and the EditorViewportRenderer-owned pipeline each have their own set. This matches the existing RSR pattern exactly and avoids FBO sharing complexity across multiple render contexts (the editor runs RSR for play-mode AND its own pipeline for edit-mode simultaneously).

### CMake Target Graph After Phase 5

```
SceneRenderPipeline  →  engine_rendering  (already has all deps)

RuntimeSceneRenderer  →  game_rendering  (composes SceneRenderPipeline, adds ECS + MaterialTextureLibrary)
EditorViewportRenderer  →  editor  (composes SceneRenderPipeline + MaterialTextureLibrary)

level-editor  →  editor  (unchanged)
procedural-model-viewer  →  game_rendering OR engine_rendering  (see note)
pixel-roguelike  →  gameplay  →  game_rendering  (unchanged)
```

The model viewer currently links against `game_rendering` and `game_content` implicitly through its includes (`game/rendering/MaterialDefinition.h`, `game/levels/cathedral/CathedralAssets.h`). SceneRenderPipeline in `engine_rendering` is already reachable from the model viewer via `game_rendering`. No CMake changes are needed for the model viewer's link line.

### Shader Ownership

The scene shader (`assets/shaders/game/scene.vert` / `scene.frag`) is loaded by SceneRenderPipeline and becomes the single canonical scene shader. The editor's current `sceneShader` (lines 299-302 in main.cpp) and the model viewer's `sceneShader` are redundant copies — they are replaced.

### Environment Panel Live Preview (D-03, D-05)

The editor's `EnvironmentPanel` already calls `document.setEnvironment(...)` per-slider. For live preview, `EditorViewportRenderer::render()` accepts a `const EnvironmentDefinition&` that it converts to a `SceneRenderInput` on every frame. This means the environment panel's changes are reflected immediately without any additional plumbing.

The new Phase 4 parameters that need UI exposure:
- SSAO: `post.ssaoRadius`, `post.ssaoBias`, `post.ssaoStrength` (exist in `PostProcessParams`)
- Bloom: `post.bloomThreshold`, `post.bloomIntensity`, `post.bloomRadius` (exist in `PostProcessParams`)
- CSM cascade distances: stored inside `CascadedShadowMap` computation — may need new fields in `EnvironmentDefinition.lighting` or `PostProcessParams`

### Asset Preview Simplified Lighting (D-12, D-13)

`EditorAssetPreviewRenderer` uses a fixed-function scene shader with hardcoded neutral lights (no LTC, no CSM). After this phase, it should:
1. Bind LTC sampler units (10/11) to real LTC textures — this enables area light support for previews
2. NOT run bloom or SSAO (thumbnail-scale has diminishing returns — decision is locked)
3. Use embedded studio lighting constants (recommended: 3-point neutral point lights, embedded as `constexpr` in EditorAssetPreviewRenderer.cpp — no .environment file needed, simpler)

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Bloom | Custom blur chain | `BloomPass` | Already in engine_rendering with mip-chain, 13-tap kernel |
| SSAO | Custom hemisphere samples | `SsaoPass` | Already in engine_rendering with noise texture, blur |
| Tonemapping | Custom ACES shader | `CompositePass` | Already handles ACES, sky, AO multiply, vignette |
| Edge stylization | Custom edge detect | `StylizePass` | Already in engine_rendering |
| CSM | Custom cascade math | `CascadedShadowMap` | Already in engine_rendering with GL_TEXTURE_2D_ARRAY geometry shader |
| LTC area lights | Custom LTC tables | `LtcData` | Already in engine_rendering with analytically-generated tables |
| Shadow slot tracking | Custom array management | `assignShadowSlots` pattern from RSR | Direct copy — identical logic in both RSR and EditorScenePreviewRenderer |
| FBO chain | Manual GL framebuffer setup | `Framebuffer` RAII class | Handles create/resize/bind/unbind, already used everywhere |

**Key insight:** 100% of the rendering primitives needed by the shared pipeline already exist in `engine_rendering`. This phase is purely an orchestration refactor — no new GL code, no new shader work.

---

## Common Pitfalls

### Pitfall 1: LTC Sampler Type Mismatch (GL_INVALID_OPERATION)
**What goes wrong:** If the scene shader runs with `uLtcMat`/`uLtcAmp` bound to 0 (null), macOS OpenGL produces `GL_INVALID_OPERATION` because the sampler type (sampler2D) doesn't match the null texture. The current editor workaround (lines 1189-1195 of main.cpp) binds null 2D textures to units 10/11 explicitly to suppress this.
**Why it happens:** macOS OpenGL 4.1 is stricter than other platforms about sampler/texture type consistency.
**How to avoid:** SceneRenderPipeline always binds real LTC textures from `LtcData`. LtcData is initialized once in `init()` — never pass null to LTC units.
**Warning signs:** `GL_INVALID_OPERATION` errors on draw, flickering or black materials with area lights.

### Pitfall 2: CSM Sampler2DArray on a null/wrong texture type
**What goes wrong:** The scene.frag uses `sampler2DArray` for `uCsmShadowMap` (unit 16). If `glBindTexture(GL_TEXTURE_2D_ARRAY, 0)` is called without also calling `glBindTexture(GL_TEXTURE_2D, 0)` on the same unit, the driver may not clear the previous binding.
**Why it happens:** OpenGL texture unit state is per-target (2D, 2D_ARRAY, CUBE, etc.) — binding a TEXTURE_2D_ARRAY to unit 16 does not unbind a TEXTURE_2D that was previously bound to the same unit.
**How to avoid:** When CSM is disabled, bind a real 1x1 depth array texture instead of 0. Or always enable CSM even at zero intensity.
**Warning signs:** Random GL errors in the CSM-enabled path when toggling CSM on/off.

### Pitfall 3: `ShadowRenderData.matrices` Array Initialization
**What goes wrong:** The old editor code initializes `shadowData.matrices` as a 2-element init list (line 1170-1172 in main.cpp). The actual array is `kMaxShadowedSpotLights = 8` elements. The 2-element init list compiled under MSVC due to aggregate initialization rules but leaves elements 2-7 uninitialized.
**Why it happens:** A brace-init list shorter than the array length leaves trailing elements value-initialized in C++20, but this is fragile — verify the RSR path uses `.fill()` which it does.
**How to avoid:** Always use `shadowData.matrices.fill(glm::mat4(1.0f))` — as RSR already does, not `{..., ...}` initialization. Fix this during the extraction.
**Warning signs:** Shadow artifacts (wrong matrices) for lights with shadowIndex >= 2.

### Pitfall 4: Viewmodel Depth Range Trick
**What goes wrong:** RSR renders viewmodel objects with `glDepthRange(0.0, 0.01)` to prevent z-fighting with world geometry. If the SceneRenderPipeline tries to render viewmodels inside the unified scene pass, they will z-fight or clip through walls.
**Why it happens:** Viewmodels have an intentional near-depth crush — they render at a fake depth range so they always appear in front of world geometry without modifying their geometry.
**How to avoid:** Keep viewmodel rendering as a separate RSR-only pass *after* the shared pipeline renders the main scene. The pipeline produces its output (the sceneFBO is populated), then RSR binds `pipeline_.sceneFBO()` again and renders viewmodels with the depth range trick before the post-process stage. Or pass viewmodels in a separate `viewmodelObjects` field in SceneRenderInput that the pipeline handles with the depth range trick internally.
**Warning signs:** Player hands disappearing into walls, or viewmodels rendering incorrectly at wrong depth.

### Pitfall 5: `geomNormalTexture()` vs `normalTexture()` in SSAO
**What goes wrong:** SsaoPass requires `sceneFBO_.geomNormalTexture()` (unperturbed geometry normals at layout location 2), not `sceneFBO_.normalTexture()` (the surface normal with normal-map perturbation). Using the wrong attachment produces SSAO artifacts.
**Why it happens:** The scene.frag outputs two separate normal attachments — one for SSAO (geometry normals, not normal-mapped) and one for edge detection in StylizePass. Per STATE.md: "Geometry normals written at layout location 2 in scene.frag for SSAO — unperturbed vNormal used."
**How to avoid:** SceneRenderPipeline::renderPostProcess must use `sceneFBO_.geomNormalTexture()` for SsaoPass and `sceneFBO_.normalTexture()` for StylizePass. This is already the correct pattern in RSR — replicate it exactly.
**Warning signs:** SSAO creating per-material artifacts or halo/glow artifacts around objects (the recent quick task 260330-kp3 was about this kind of artifact).

### Pitfall 6: CMake include_directories and game-layer types in engine_rendering
**What goes wrong:** SceneRenderPipeline in `engine_rendering` must NOT include game-layer headers (`game/rendering/MaterialKind.h`, etc.). The architecture rule is: engine layer has no game dependencies.
**Why it happens:** `RenderMaterialData` in `engine/rendering/geometry/Renderer.h` currently includes `game/rendering/MaterialKind.h`. This is an existing violation to be aware of — it already compiled because game_content links engine_rendering. The SceneRenderPipeline should not make this worse.
**How to avoid:** SceneRenderPipeline's interface takes `RenderObject` (already engine-layer), `RenderLight` (already engine-layer), `LightingEnvironment` (already engine-layer), `PostProcessParams` (already engine-layer). No game-layer types in SceneRenderPipeline's header.
**Warning signs:** CMake link errors or circular dependency errors if SceneRenderPipeline accidentally pulls in game_content.

### Pitfall 7: Editor ShadowRenderData textures array vs RSR
**What goes wrong:** `EditorScenePreviewRenderer.cpp::renderShadowPass` (free function) initializes `shadowData.textures` from `shadowMaps[i].texture()` for all 8 slots up front. RSR does the same. If SceneRenderPipeline's shadow pass omits this pre-initialization, lights with no shadow (shadowIndex=-1) may sample garbage from uninitialized texture handles.
**Why it happens:** The Renderer::drawScene binds shadow textures by index. Slots with no shadow should sample a valid (if dummy) depth texture, or the shader must guard with `if (shadowIndex >= 0)`.
**How to avoid:** Keep the pre-initialization loop in SceneRenderPipeline (from RSR): populate `shadowData.textures[i] = shadowMaps_[i].texture()` for all 8 slots before rendering shadows, so unassigned slots have a valid (if unconfigured) texture handle.

---

## Code Examples

### Pattern 1: Exact stubs to replace in editor main.cpp (lines 1180-1215)

```cpp
// Current stub code (lines 1180-1215) — REPLACE with pipeline_.render(input, ...)
sceneShader->setInt("uCsmEnabled", 0);           // stub: CSM disabled
sceneShader->setInt("uCsmCascadeCount", 0);      // stub: no cascades
sceneShader->setInt("uCsmShadowMap", 16);
glActiveTexture(GL_TEXTURE16);
glBindTexture(GL_TEXTURE_2D_ARRAY, 0);           // null CSM texture
sceneShader->setInt("uLtcMat", 10);
sceneShader->setInt("uLtcAmp", 11);
glActiveTexture(GL_TEXTURE10);
glBindTexture(GL_TEXTURE_2D, 0);                 // null LTC mat
glActiveTexture(GL_TEXTURE11);
glBindTexture(GL_TEXTURE_2D, 0);                 // null LTC amp
...
compositePass.apply(..., 0,  // no bloom in editor preview
                         0,  // no SSAO in editor preview
                         ...);
```

### Pattern 2: RSR renderScenePass — CSM + LTC binding (to move into SceneRenderPipeline)

```cpp
// Source: src/game/rendering/RuntimeSceneRenderer.cpp::renderScenePass
constexpr int kCsmTextureUnit = 16;
sceneShader_->setInt("uCsmShadowMap", kCsmTextureUnit);
sceneShader_->setInt("uCsmEnabled", csmEnabled ? 1 : 0);
sceneShader_->setInt("uCsmCascadeCount", CascadedShadowMap::kCascadeCount);
glActiveTexture(GL_TEXTURE0 + kCsmTextureUnit);
glBindTexture(GL_TEXTURE_2D_ARRAY, csmEnabled ? csmShadowMap_.depthArrayTexture() : 0);

constexpr int kLtcMatUnit = 10;
constexpr int kLtcAmpUnit = 11;
glActiveTexture(GL_TEXTURE0 + kLtcMatUnit);
glBindTexture(GL_TEXTURE_2D, ltcData_.ltcMatTexture());
sceneShader_->setInt("uLtcMat", kLtcMatUnit);
glActiveTexture(GL_TEXTURE0 + kLtcAmpUnit);
glBindTexture(GL_TEXTURE_2D, ltcData_.ltcAmpTexture());
sceneShader_->setInt("uLtcAmp", kLtcAmpUnit);
```

### Pattern 3: RSR renderPostProcess (to move into SceneRenderPipeline)

```cpp
// Source: src/game/rendering/RuntimeSceneRenderer.cpp::renderPostProcess
bloomPass_.render(sceneFBO_.colorTexture(),
                  params.post.bloomRadius * 0.003f,
                  params.post.bloomThreshold);

if (params.post.enableSsao) {
    ssaoPass_.render(sceneFBO_.depthTexture(),
                     sceneFBO_.geomNormalTexture(),  // NOT normalTexture()
                     camera.projectionMatrix,
                     camera.viewMatrix,
                     params.post.ssaoRadius,
                     params.post.ssaoBias,
                     params.post.ssaoStrength);
}

compositePass_.apply(sceneFBO_.colorTexture(),
                     sceneFBO_.depthTexture(),
                     sceneFBO_.normalTexture(),
                     bloomPass_.bloomTexture(),
                     params.post.enableSsao ? ssaoPass_.aoTexture() : 0,
                     compositeFBO_.framebuffer(),
                     params.post,
                     compositeFBO_.width(),
                     compositeFBO_.height());

stylizePass_.apply(compositeFBO_.colorTexture(),
                   sceneFBO_.colorTexture(),
                   sceneFBO_.depthTexture(),
                   sceneFBO_.normalTexture(),
                   params.post,
                   outputWidth, outputHeight,
                   targetFramebuffer);
```

### Pattern 4: Model viewer — current rendering (to replace with SceneRenderPipeline)

```cpp
// Source: apps/model_viewer/main.cpp lines 315-344
// Currently missing: CSM, LTC, SSAO, bloom
// CompositePass.apply() receives 0,0 for bloom/ssao (overloaded signature without those args)
compositePass.apply(sceneFbo.colorTexture(), sceneFbo.depthTexture(),
                    sceneFbo.normalTexture(),
                    compositeFbo.framebuffer(),   // <-- older overload without bloom/ssao params
                    params, compositeFbo.width(), compositeFbo.height());
```

Note: The model viewer calls a different overload of `CompositePass::apply()` — one without bloom/ssao texture params. After Phase 5, it will use SceneRenderPipeline which uses the full 9-arg overload.

### Pattern 5: Duplicate shadow slot / shadow matrix logic (both to be replaced by shared)

EditorScenePreviewRenderer.cpp has `assignShadowSlots`, `buildShadowMatrix`, `renderShadowPass` as local free functions — exact duplicates of RSR's private methods. Both implementations are identical. SceneRenderPipeline will be the single canonical implementation of all three.

### Pattern 6: Performance stats pattern to extend (D-16)

```cpp
// Source: src/game/runtime/RuntimeGameSession.h
struct RuntimeSessionPerformanceStats {
    double rebuildMs = 0.0;
    double resetForPlayMs = 0.0;
    double rendererInitMs = 0.0;
    double rendererPrewarmMs = 0.0;
    double lastRenderMs = 0.0;
};
// Extend with per-effect timings for editor viewport overlay:
struct SceneRenderPipelineStats {
    double totalRenderMs = 0.0;
    double shadowPassMs = 0.0;
    double scenePassMs = 0.0;
    double bloomMs = 0.0;
    double ssaoMs = 0.0;
    double compositeMs = 0.0;
    int drawCalls = 0;
};
```

---

## Implementation Order (Recommended)

1. **Extract SceneRenderPipeline** — move shadow pass, CSM pass, scene pass, bloom, SSAO, composite, stylize from RSR into `SceneRenderPipeline`. Add it to `engine_rendering` CMakeLists.
2. **Wire RSR to SceneRenderPipeline** — RSR composes the pipeline, retains only ECS data collection, player torch, viewmodel rendering, environment sync. Build and test: runtime game must look identical.
3. **Extract EditorViewportRenderer** — move ~lines 298-1224 of main.cpp into new class. Wire to SceneRenderPipeline. Build and test: editor edit-mode now has full Phase 4 rendering.
4. **Extend environment panel** — expose SSAO radius/intensity/bias, bloom threshold/intensity, CSM cascade distances.
5. **Wire model viewer** — replace inline rendering in model_viewer/main.cpp with SceneRenderPipeline or EditorViewportRenderer (simpler: just use SceneRenderPipeline directly, no editor overlays needed).
6. **Performance overlay** — add SceneRenderPipelineStats to SceneRenderPipeline, expose in EditorViewportRenderer's ImGui overlay.
7. **Asset preview LTC** — update EditorAssetPreviewRenderer to bind real LTC textures from a shared LtcData instance.

---

## State of the Art

| Old Approach (Pre-Phase 5) | Current Approach (Post-Phase 5) | Impact |
|---------------------------|--------------------------------|--------|
| Inline rendering in editor main.cpp | EditorViewportRenderer class | main.cpp drops ~300 rendering lines |
| Duplicate shadow logic (RSR + editor) | Single SceneRenderPipeline impl | One place to fix shadow bugs |
| Duplicate `buildShadowMatrix`, `assignShadowSlots` | Single implementation in SceneRenderPipeline | Eliminates drift between editor and runtime |
| Editor stubs: uCsmEnabled=0, LTC null-bound | Full CSM + LTC in editor | What you see in editor = what you get in game |
| Model viewer: bloom disabled via TAB | Bloom + SSAO available in model viewer | Accurate preview when building materials |

---

## Open Questions

1. **CompositePass overload mismatch in model viewer**
   - What we know: model_viewer/main.cpp calls `compositePass.apply()` with 7 args (without bloom/ssao texture args). RSR calls with 9 args (with bloom and ssao textures). CompositePass.h shows only one signature with 9 args.
   - What's unclear: The model viewer compiles — there must be another overload or the missing args default. Need to verify at compile time.
   - Recommendation: SceneRenderPipeline uses the 9-arg overload (bloom+ssao). The model viewer's old call disappears when it switches to SceneRenderPipeline.

2. **DebugParams vs PostProcessParams for per-effect toggles (D-14)**
   - What we know: DebugParams already has `shadowsEnabled`, `shadowMapResolutionIndex`. PostProcessParams already has `enableSsao`, `enableBloom`. DebugParams wraps PostProcessParams as a field (`post`).
   - What's unclear: Does the editor use DebugParams or EnvironmentDefinition for toggle state? The editor uses `EnvironmentDefinition` (not `DebugParams`) for its environment. SceneRenderInput will need to thread the right toggle source.
   - Recommendation: SceneRenderInput takes a `bool shadowsEnabled` + `const PostProcessParams*` directly. The caller (EditorViewportRenderer) maps from its toggle UI state to these fields.

3. **CSM cascade distances — current exposure**
   - What we know: `CascadedShadowMap::computeCascades()` takes `nearPlane` and `farPlane` but computes splits internally using a logarithmic/linear blend. There are no per-cascade distance fields in `PostProcessParams` or `EnvironmentDefinition` yet.
   - What's unclear: The CONTEXT.md (D-05) says the environment panel should expose "CSM cascade distances" — but there are no such fields yet.
   - Recommendation: Add `float csmLambda = 0.5f` (the PSSM split blend factor) to `PostProcessParams`. This is a single float that controls the logarithmic/linear blend across all 3 cascades — the most practical UI parameter. Full individual cascade distances can be deferred.

---

## Environment Availability

Step 2.6: SKIPPED — this phase is purely source code changes (C++ refactor + new class creation). No external tools, services, or runtime dependencies beyond the existing build chain.

---

## Project Constraints (from CLAUDE.md)

- **Engine:** Custom C++ — no Unity/Unreal/Godot
- **Graphics API:** OpenGL 4.1 Core Profile — all shaders use `#version 410 core`
- **GLSL max version:** 4.10 (macOS caps at 4.1)
- **Build:** CMake with FetchContent; GLFW/GLM/spdlog via Homebrew
- **Naming:** Classes PascalCase, functions camelCase, private members trailing underscore, constants `k` prefix + PascalCase
- **Headers:** `#pragma once`, include order: stdlib → third-party → project headers
- **Classes:** Non-copyable by default (`= delete`), RAII for OpenGL resources, virtual destructors on base classes
- **ECS:** Components are POD structs, systems inherit from `System` base, `UpdatePhase` ordering
- **No GLEW** — use GLAD v2
- **No Boost** — use C++20 STL
- **No Bullet3** — use Jolt Physics v5
- **No OpenGL compatibility profile** — Core profile only
- **Code style:** `.clang-format` LLVM, 4-space indent, 100-char column, attached braces, left pointer alignment
- **GSD Workflow:** Use `/gsd:execute-phase` entry point for all repo edits

---

## Sources

### Primary (HIGH confidence — direct codebase analysis)
- `src/game/rendering/RuntimeSceneRenderer.h` / `.cpp` — full pipeline implementation, source of truth
- `src/editor/render/EditorScenePreviewRenderer.h` / `.cpp` — duplicate shadow logic, free functions to consolidate
- `apps/level_editor/main.cpp` (lines 298-320, 1165-1223) — inline rendering stubs to replace
- `apps/model_viewer/main.cpp` (lines 161-344) — model viewer rendering to upgrade
- `src/engine/rendering/post/` — BloomPass, SsaoPass, CompositePass, StylizePass headers
- `src/engine/rendering/lighting/` — CascadedShadowMap, LtcData, RenderLight headers
- `src/engine/rendering/geometry/Renderer.h` — RenderObject, ShadowRenderData, LightingEnvironment
- `src/engine/rendering/post/PostProcessParams.h` — all post-process parameter fields
- `src/engine/ui/ImGuiLayer.h` — DebugParams struct
- `src/game/rendering/EnvironmentDebugSync.h` — RuntimeEnvironmentSyncState, syncEnvironmentFromRegistry
- `src/game/rendering/EnvironmentDefinition.h` — EnvironmentDefinition struct
- `src/engine/CMakeLists.txt` / `src/game/CMakeLists.txt` / `src/editor/CMakeLists.txt` — target graph
- `.planning/STATE.md` — Phase 4 decisions, macOS OpenGL constraints

### No External Sources Required
This phase is a pure codebase refactor. All necessary knowledge comes from reading the existing source. No third-party documentation or web searches were needed.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries already in codebase, no new dependencies
- Architecture: HIGH — SceneRenderPipeline design derived from direct RSR analysis; composition pattern matches EditorRuntimePreviewSession precedent
- Pitfalls: HIGH — all pitfalls identified from actual code inspection (null sampler bindings, geomNormal vs normal, depth range trick, array init)
- Open questions: MEDIUM — 3 minor ambiguities, all have viable recommended resolutions

**Research date:** 2026-03-30
**Valid until:** 2026-04-30 (stable codebase, no external dependencies)
