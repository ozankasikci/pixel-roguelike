# Phase 12: Engine Quality — Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Seven engine-quality improvements that make the rendering pipeline more robust, the asset system data-driven, and the core systems safer. No user-facing behavior changes — all improvements are internal architecture and performance.

Delivers: frustum culling, texture unit enum, file-based mesh discovery, EventBus RAII tokens, DebugParams decomposition, LevelLoader unification, and GenericFileScene scripted-geometry extraction.

</domain>

<decisions>
## Implementation Decisions

### Frustum Culling
- **D-01:** AABB frustum culling only — no material/shader sort batching in this phase
- **D-02:** Culling happens at the `SceneRenderPipeline` level so it applies to all render paths (runtime, editor viewport, model viewer) — this is industry standard
- **D-03:** Light culling (skipping lights whose attenuation radius doesn't overlap the frustum) — Claude's discretion based on complexity vs benefit

### Texture Unit Enum
- **D-04:** Replace all magic-number texture unit indices (shadow maps 8-15, albedo 12, normal 13, roughness 14, AO 15, LTC mat 10, LTC amp 11, CSM 16) with a named enum in a shared header. Use those names everywhere across Renderer.cpp, SceneRenderPipeline.cpp, SsaoPass.cpp, etc.

### Generic Asset System (File-Based Mesh Discovery)
- **D-05:** Meshes in `assets/meshes/` should be auto-discovered from the filesystem at startup, like materials already are (Phase 7). No manual code registration for file-based meshes.
- **D-06:** Procedural meshes (cathedral pillars, prison walls, HVAC vents, chain padlock, smoke detector, etc.) stay code-generated but move to `ProceduralGameAssets` (renamed from `GameAssets`). These register into `MeshLibrary` alongside file-discovered meshes.
- **D-07:** New assets should be files whenever possible. Procedural generation is reserved for geometry that's cheaper to generate than model.
- **D-08:** No lazy/deferred loading in this phase — keep synchronous loading. Lazy loading is a future concern.

### EventBus RAII Tokens
- **D-09:** `EventBus::subscribe()` returns an RAII subscription token that unregisters the handler on destruction. Matches patterns used in Godot's signal system and Hazel's event system.

### DebugParams Split (from CONCERNS.md)
- **D-10:** Decompose the 40-field `DebugParams` struct into `PostProcessParams`, `CameraDebugInfo`, and `RuntimeLightingOverride`. Reduces recompilation blast radius across the 11 files that currently depend on it.

### LevelLoader Unification (from CONCERNS.md)
- **D-11:** Merge the two `LevelLoader::load()` overloads (Application path vs registry path) into a single overload that takes an explicit context struct. Prevents feature divergence between editor and runtime loading paths.

### GenericFileScene Hard-Coded Branches (from CONCERNS.md)
- **D-12:** Move the `if (request_.levelId == "institutional_room")` scripted geometry injection out of `GenericFileScene::onEnter()` into per-level callbacks or encode the objects in the `.scene` file itself.

### Claude's Discretion
- Light frustum culling implementation (D-03) — evaluate whether the win justifies the complexity given the 32-light cap
- Texture/environment unification into the discovery pattern — Claude evaluates whether it makes sense beyond meshes
- Priority ordering across the 7 items — all equal priority, Claude orders by dependency analysis

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Rendering Pipeline
- `src/engine/rendering/SceneRenderPipeline.cpp` — Shared render pipeline (frustum culling entry point, texture unit assignments)
- `src/engine/rendering/SceneRenderPipeline.h` — Pipeline interface and constants
- `src/engine/rendering/geometry/Renderer.cpp` — Draw call submission, texture unit magic numbers
- `src/engine/rendering/geometry/Renderer.h` — Renderer interface
- `src/engine/rendering/lighting/RenderLight.h` — kMaxRenderLights, kMaxShadowedSpotLights constants

### Post-Process Passes (texture unit usage)
- `src/engine/rendering/post/SsaoPass.cpp` — SSAO texture unit bindings
- `src/engine/rendering/post/BloomPass.cpp` — Bloom texture unit bindings
- `src/engine/rendering/post/StylizePass.cpp` — Stylize texture unit bindings
- `src/engine/rendering/post/CompositePass.cpp` — Composite texture unit bindings
- `src/editor/render/EditorAssetPreviewRenderer.cpp` — Editor texture unit bindings

### Asset System
- `src/game/levels/GameAssets.cpp` — 880-line monolith to be split (→ ProceduralGameAssets)
- `src/engine/rendering/assets/AssetCache.cpp` — Binary mesh cache
- `src/engine/rendering/geometry/MeshLibrary.h` — Name→mesh registry
- `src/game/content/ContentRegistry.cpp` — Material/environment/weapon loaders (reference for discovery pattern)

### EventBus
- `src/engine/core/EventBus.h` — Current subscribe-only implementation (31 lines, no unsubscription)
- `src/engine/core/Application.h` — EventBus ownership and accessor

### DebugParams
- `src/engine/ui/ImGuiLayer.h` — DebugParams struct definition (lines 26-65)

### LevelLoader
- `src/game/level/LevelLoader.h` — Dual load() overload signatures
- `src/game/level/LevelLoader.cpp` — Implementation of both paths

### GenericFileScene
- `src/game/scenes/GenericFileScene.cpp` — Hard-coded institutional_room branch

### Codebase Analysis
- `.planning/codebase/CONCERNS.md` — Full codebase concerns audit (source of D-10 through D-12)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mesh->aabbMin()` / `mesh->aabbMax()` — AABB bounds already exist on all meshes, ready for frustum testing
- `ContentRegistry` material auto-discovery pattern — model for how mesh discovery should work
- `AssetCache` — binary caching infrastructure already in place for meshes

### Established Patterns
- Materials use file-based auto-discovery from `assets/materials/` (Phase 7) — meshes should follow the same pattern
- `SceneRenderPipeline` is shared across runtime/editor/model viewer — natural place for culling
- Systems use `init()` / `update()` / `shutdown()` lifecycle from `System` base class
- RAII used throughout for OpenGL resources (VAO/VBO/EBO/FBO destructors)

### Integration Points
- `RuntimeSceneRenderer::collectSceneObjects()` — where frustum culling filters before draw submission
- `MeshLibrary::registerMesh()` — entry point for both file-discovered and procedural meshes
- All `glActiveTexture(GL_TEXTURE0 + N)` call sites — need to use enum values
- `EventBus::subscribe()` callers — need to capture returned tokens

</code_context>

<specifics>
## Specific Ideas

- User explicitly wants the asset system to work "like Unity and Unreal" — scan filesystem, auto-discover, no hardcoded registration
- Procedural meshes renamed to `ProceduralGameAssets` (not deleted, not deprecated — explicit name for their role)
- The texture unit enum was specifically called out in CONCERNS.md with suggested enum values: `TextureUnit { kShadowMap0 = 8, kAlbedo = 12, ... }`

</specifics>

<deferred>
## Deferred Ideas

- Material/shader sort batching — reduces GPU state changes but adds complexity; save for when draw call count becomes a bottleneck
- Lazy/deferred asset loading — background loading and asset streaming; save for when load times become a real problem
- Scene file versioning — `.scene` format has no version header; separate concern from asset discovery
- Component validation / schema enforcement — referential integrity checks at load time; not part of this phase

</deferred>

---

*Phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens*
*Context gathered: 2026-04-01*
