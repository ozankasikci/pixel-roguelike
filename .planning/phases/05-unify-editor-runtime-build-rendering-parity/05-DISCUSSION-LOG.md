# Phase 5: Unify Editor/Runtime/Build Rendering Parity - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-30
**Phase:** 05-unify-editor-runtime-build-rendering-parity
**Areas discussed:** Rendering scope, Shared vs duplicated code, Asset preview parity, Performance strategy

---

## Rendering Scope

### Which Phase 4 features should the editor edit-mode viewport get?

| Option | Description | Selected |
|--------|-------------|----------|
| Full parity (Recommended) | Bring ALL Phase 4 features into the editor viewport: bloom, SSAO, CSM directional shadows, LTC area lights, emissive materials. WYSIWYG when editing. | ✓ |
| Visual-only subset | Bloom + SSAO + emissive (biggest visual difference). Skip CSM and LTC. | |
| You decide | Claude picks based on implementation complexity and visual impact. | |

**User's choice:** Full parity (Recommended)

### Should the editor viewport support all light types from Phase 4 in edit-mode?

| Option | Description | Selected |
|--------|-------------|----------|
| All light types | Area lights (LTC), tube lights, emissive surfaces — all render correctly in the editor viewport. | ✓ |
| Point/spot/directional only | Keep current light types in edit-mode. Area/tube render as point lights until Play. | |
| You decide | Claude picks based on what shared code path naturally supports. | |

**User's choice:** All light types

### Should environment panel adjustments render live in the viewport?

| Option | Description | Selected |
|--------|-------------|----------|
| Live preview (Recommended) | Environment panel adjustments reflect instantly in the viewport. | ✓ |
| Apply on save only | Changes only render after saving the environment definition. | |

**User's choice:** Live preview (Recommended)

### Should debug view modes include new post-process features?

| Option | Description | Selected |
|--------|-------------|----------|
| Extend debug modes | Add SSAO-only, Bloom-only, Shadow-cascades debug view modes. | |
| Keep existing modes | Current debug modes (Final, Lighting Only, Sky Only) stay as-is. | ✓ |
| You decide | Claude determines what debug views are useful. | |

**User's choice:** Keep existing modes

### Should environment panel expose new Phase 4 parameters?

| Option | Description | Selected |
|--------|-------------|----------|
| Expose all new params (Recommended) | Add SSAO, bloom, and CSM controls to environment panel. | ✓ |
| Use defaults only | New features use hardcoded defaults in the editor. | |
| You decide | Claude picks which params are worth exposing. | |

**User's choice:** Expose all new params (Recommended)

---

## Shared vs Duplicated Code

### How should the editor viewport get the full rendering pipeline?

| Option | Description | Selected |
|--------|-------------|----------|
| Reuse RuntimeSceneRenderer | Editor creates its own RSR instance. Minimal duplication but RSR expects game patterns. | |
| Extract shared render pipeline | Factor out SceneRenderPipeline that both editor and runtime compose. Cleaner long-term. | ✓ |
| Wire passes inline | Add BloomPass, SsaoPass, etc. to main.cpp locally. Fast but duplicates setup logic. | |
| You decide | Claude picks based on code complexity and maintenance cost. | |

**User's choice:** Extract shared render pipeline

### Where should the shared pipeline live?

| Option | Description | Selected |
|--------|-------------|----------|
| Engine layer (Recommended) | SceneRenderPipeline in src/engine/rendering/. Both game and editor depend on engine. | ✓ |
| Game layer | In src/game/rendering/ alongside RSR. Closer to existing code but wrong dependency direction. | |
| You decide | Claude picks based on dependency graph. | |

**User's choice:** Engine layer (Recommended)

### Should editor free functions be refactored into shared pipeline?

| Option | Description | Selected |
|--------|-------------|----------|
| Refactor into shared (Recommended) | collectLights and renderShadowPass have near-identical logic. Move shared logic to pipeline. | ✓ |
| Keep as adapters | Editor keeps own functions but calls pipeline for heavy lifting. | |
| You decide | Claude picks based on overlap. | |

**User's choice:** Refactor into shared (Recommended)

### Should RuntimeSceneRenderer be refactored to compose the new shared pipeline?

| Option | Description | Selected |
|--------|-------------|----------|
| Refactor RSR to compose pipeline (Recommended) | RSR becomes thin wrapper around SceneRenderPipeline + game-specific logic. Single source of truth. | ✓ |
| Keep RSR as-is, extract separately | Don't touch RSR. Extract pipeline as new code. Safer but risks drift. | |
| You decide | Claude picks based on risk assessment. | |

**User's choice:** Refactor RSR to compose pipeline (Recommended)

### Should editor main.cpp rendering be extracted into a class?

| Option | Description | Selected |
|--------|-------------|----------|
| Extract EditorViewportRenderer (Recommended) | Create class composing shared pipeline + editor overlays. Cleans up main.cpp. | ✓ |
| Keep inline, just wire pipeline | Wire SceneRenderPipeline into existing inline code. Less refactoring. | |
| You decide | Claude picks based on natural cleanup. | |

**User's choice:** Extract EditorViewportRenderer (Recommended)

### Should the model viewer also be updated?

| Option | Description | Selected |
|--------|-------------|----------|
| Include model viewer | Three executables, one shared pipeline. All produce identical output. | ✓ |
| Editor + runtime only (Recommended) | Focus on two main apps. Model viewer can be a follow-up. | |
| You decide | Claude picks based on model viewer complexity. | |

**User's choice:** Include model viewer

### How should editor overlays integrate with the shared pipeline?

| Option | Description | Selected |
|--------|-------------|----------|
| Overlay callback hook | Pipeline provides hook between scene pass and post-process. Editor injects overlays. | |
| Additional render objects | Editor adds overlay objects with special material flag. | |
| You decide | Claude picks cleanest integration point. | ✓ |

**User's choice:** You decide (Claude's discretion)

---

## Asset Preview Parity

### Should asset previews get the full pipeline?

| Option | Description | Selected |
|--------|-------------|----------|
| Full pipeline for asset previews | Mesh/material previews with bloom, SSAO, correct lighting. | |
| Simplified lighting only (Recommended) | Correct light types and emissive, but no SSAO/bloom. Diminishing returns at thumbnail size. | ✓ |
| Keep as-is | Asset previews stay basic. Full parity in viewport is enough. | |
| You decide | Claude picks based on what naturally falls out of shared pipeline work. | |

**User's choice:** Simplified lighting only (Recommended)

### Should asset preview lighting use current scene's environment?

| Option | Description | Selected |
|--------|-------------|----------|
| Current scene environment (Recommended) | Material previews reflect scene's lighting for in-context preview. | |
| Fixed preview environment | Neutral studio lighting. Consistent regardless of scene. | ✓ |
| You decide | Claude picks based on wiring ease. | |

**User's choice:** Fixed preview environment

---

## Performance Strategy

### How should performance be managed with the full pipeline in editor?

| Option | Description | Selected |
|--------|-------------|----------|
| Per-effect toggles (Recommended) | Individual on/off for bloom, SSAO, CSM, shadows. All on by default. DebugParams has toggle fields. | ✓ |
| Quality presets | Low/Medium/High presets controlling effect resolution and enabling. | |
| Auto-scale | Monitor frame time and auto-downgrade effects when slow. | |
| You decide | Claude picks simplest effective approach. | |

**User's choice:** Per-effect toggles (Recommended)

### Should the editor render at reduced internal resolution?

| Option | Description | Selected |
|--------|-------------|----------|
| Native resolution always | Render at actual pixel size. Modern GPUs handle it. | ✓ |
| Configurable render scale (Recommended) | Render scale slider (50%-200%). Runtime already supports internal vs output resolution. | |
| You decide | Claude picks based on existing resolution handling. | |

**User's choice:** Native resolution always

### Should the editor display performance stats?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, in viewport overlay | Frame time, draw calls, per-effect timing in viewport corner. Extend RuntimeSessionPerformanceStats. | ✓ |
| ImGui debug panel only | Stats in separate ImGui window, not viewport overlay. | |
| You decide | Claude picks based on existing patterns. | |

**User's choice:** Yes, in viewport overlay

---

## Claude's Discretion

- SceneRenderPipeline class API design and initialization interface
- Editor overlay integration approach (callback hook, render object flags, or separate pass)
- FBO management strategy
- CMake target dependencies
- Implementation order
- Neutral studio lighting for asset previews (constants vs file)

## Deferred Ideas

- Render scale slider
- Per-effect debug view modes (SSAO-only, Bloom-only, Cascade visualization)
- Full post-processing for asset thumbnails
- Auto-scaling quality based on frame time
- Quality presets (Low/Medium/High)
