# Phase 5: Unify Editor/Runtime/Build Rendering Parity - Context

**Gathered:** 2026-03-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Extract a shared SceneRenderPipeline from RuntimeSceneRenderer so the editor viewport, runtime game, and model viewer all render identically through the same code path — including bloom, SSAO, cascaded shadow maps, LTC area lights, tube lights, and emissive materials. The editor edit-mode viewport currently bypasses all Phase 4 rendering features. After this phase, what you see in the editor is what you get in the game.

</domain>

<decisions>
## Implementation Decisions

### Rendering Scope
- **D-01:** Full parity — bring ALL Phase 4 features into the editor viewport: bloom (mip-chain), SSAO, CSM directional shadows, LTC area lights, tube lights, and emissive material support
- **D-02:** All light types render correctly in edit-mode — area lights (LTC), tube lights, emissive surfaces all show accurate lighting while placing fixtures
- **D-03:** Environment panel adjustments (exposure, bloom intensity, AO strength, fog, CSM cascade distances) reflect instantly in the viewport — live preview, not apply-on-save
- **D-04:** Keep existing debug view modes (Final, Lighting Only, Sky Only) as-is — no new debug views for individual post-process effects
- **D-05:** Environment panel exposes all new Phase 4 parameters (SSAO radius/intensity, bloom threshold/intensity, CSM cascade distances) for interactive tuning

### Architecture — Shared Render Pipeline
- **D-06:** Extract a `SceneRenderPipeline` class in `src/engine/rendering/` that encapsulates the full rendering pipeline: scene pass, shadow pass (spot + CSM), SSAO, bloom, composite, stylize
- **D-07:** Refactor `RuntimeSceneRenderer` to compose `SceneRenderPipeline` — RSR becomes a thin wrapper adding game-specific logic (viewmodel rendering, runtime camera capture)
- **D-08:** Refactor shared logic (`collectLights`, `renderShadowPass`, shadow slot assignment) into the shared pipeline. Keep only editor-specific helpers (selection overlays, gizmos, collider wireframes) in editor code
- **D-09:** Extract `EditorViewportRenderer` class from the ~800 lines of inline rendering in `apps/level_editor/main.cpp`. This class composes `SceneRenderPipeline` + editor-specific overlays
- **D-10:** Include the model viewer (`procedural-model-viewer`) — all three executables use the same shared pipeline for identical output
- **D-11:** Editor overlay integration (gizmos, selection highlights, wireframes) — Claude's discretion on the cleanest integration point (callback hook vs render object flags vs separate pass)

### Asset Preview Parity
- **D-12:** Asset previews (mesh/material thumbnails in EditorAssetPreviewRenderer) get simplified lighting: correct light types and emissive support, but no SSAO and no bloom
- **D-13:** Asset previews use a fixed neutral studio lighting environment, not the current scene's environment definition

### Performance
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

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Runtime rendering pipeline (source of truth for extraction)
- `src/game/rendering/RuntimeSceneRenderer.h` — Full rendering pipeline: shadow pass, scene pass, bloom, SSAO, CSM, LTC, composite, stylize
- `src/game/rendering/RuntimeSceneRenderer.cpp` — Implementation with all Phase 4 features
- `assets/shaders/game/scene.vert` — Vertex shader (shared by all renderers)
- `assets/shaders/game/scene.frag` — PBR lighting shader: Cook-Torrance BRDF, 32-light loop, shadow sampling, CSM, LTC area lights, emissive

### Editor rendering (code to refactor)
- `apps/level_editor/main.cpp` — Editor main loop with inline rendering (~lines 1150-1220), shadow pass, scene pass, composite, stylize — missing bloom, SSAO, CSM, LTC
- `src/editor/render/EditorScenePreviewRenderer.h` — Free functions: collectLights, collectRenderObjects, renderShadowPass, appendHelperObjects
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Editor rendering implementation
- `src/editor/render/EditorAssetPreviewRenderer.h` — Mesh/material preview renderer (basic shader only)
- `src/editor/core/EditorRuntimePreviewSession.h` — Play-mode wrapper around RuntimeGameSession (already has full pipeline)

### Post-processing passes (to be shared)
- `src/engine/rendering/post/BloomPass.h` — Mip-chain bloom (5 half-res FBOs, 13-tap downsample, tent upsample)
- `src/engine/rendering/post/SsaoPass.h` — SSAO (32-sample hemisphere with blur)
- `src/engine/rendering/post/CompositePass.h` — Tonemapping, bloom compositing, AO application
- `src/engine/rendering/post/StylizePass.h` — Edge detection stylization
- `src/engine/rendering/lighting/CascadedShadowMap.h` — CSM (3 cascades, geometry shader)
- `src/engine/rendering/lighting/LtcData.h` — LTC lookup tables for area lights

### Environment and content systems
- `src/game/rendering/EnvironmentDefinition.h` — Environment definition with post-process params
- `src/engine/rendering/post/PostProcessParams.h` — Post-process parameter struct
- `src/engine/rendering/lighting/RenderLight.h` — Light data structures, LightType enum
- `src/game/rendering/MaterialTextureLibrary.h` — Material texture resolution

### Model viewer (third consumer)
- `apps/model_viewer/` — Model viewer app directory

### Project context
- `.planning/PROJECT.md` — Core value: Stanley Parable aesthetic, OpenGL 4.1 constraint
- `.planning/ROADMAP.md` — Phase 5 goal and dependencies

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **RuntimeSceneRenderer**: Owns the complete Phase 4 pipeline — this IS the code being extracted into the shared pipeline
- **BloomPass, SsaoPass, CompositePass, StylizePass**: Already engine-layer classes. Pipeline extraction wraps orchestration, not the passes themselves
- **CascadedShadowMap, LtcData**: Engine-layer classes for CSM and area light lookup tables
- **DebugParams**: Already has toggle fields for shadows, bloom, SSAO — the per-effect toggle mechanism exists
- **EditorRuntimePreviewSession**: Play-mode already wraps RuntimeGameSession — demonstrates the composition pattern to follow

### Established Patterns
- **Engine/game/editor layer separation**: Engine provides primitives, game composes them, editor adapts for editing workflows
- **Renderer class**: `src/engine/rendering/geometry/Renderer.h` — low-level draw call batching, used by all render paths
- **FBO pattern**: Framebuffer RAII class in engine/rendering/core — scene FBO + composite FBO + final FBO chain
- **Post-process chain**: Scene FBO → bloom → SSAO → composite FBO → stylize → screen
- **EnvironmentDefinition**: Per-scene post-process and lighting params — already consumed by composite and stylize passes

### Integration Points
- **Editor main.cpp lines 1180-1215**: Where CSM, LTC, bloom, SSAO are currently stubbed out — this is the code that gets replaced by the shared pipeline
- **Editor environment panel**: Already renders sliders for post-process params — extend with new Phase 4 controls
- **CMake target graph**: editor → gameplay → game_rendering → engine_rendering. SceneRenderPipeline in engine_rendering is reachable from all consumers
- **Model viewer app**: Has its own rendering — needs to compose the shared pipeline like the editor does

</code_context>

<specifics>
## Specific Ideas

- The editor currently has explicit stubs: `sceneShader->setInt("uCsmEnabled", 0)`, `0 // no bloom in editor preview`, `0 // no SSAO in editor preview` — these are the exact gaps to fill
- EditorRuntimePreviewSession (play-mode) already proves that the editor CAN run the full pipeline — this phase makes edit-mode match
- The LTC sampler units (10/11) are currently bound to null textures in the editor to prevent GL errors — the shared pipeline should handle this properly
- Performance overlay in viewport corner: frame time + draw calls + individual effect timings — similar to game engines like Unity's Stats window

</specifics>

<deferred>
## Deferred Ideas

- Render scale slider for weaker hardware — native resolution only for now
- SSAO-only / Bloom-only / Shadow-cascades debug view modes — not needed this phase
- Asset preview full post-processing (SSAO + bloom at thumbnail size has diminishing returns)
- Auto-scaling quality based on frame time monitoring
- Quality presets (Low/Medium/High) for the editor

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-unify-editor-runtime-build-rendering-parity*
*Context gathered: 2026-03-30*
