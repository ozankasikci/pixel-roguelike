# Phase 4: Improve Lighting Quality - Context

**Gathered:** 2026-03-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Research industry best practices for real-time lighting in OpenGL engines and implement improvements to the rendering pipeline. The goal is to bring lighting quality up to The Stanley Parable standard — soft, warm, atmospheric lighting with proper shadows, ambient occlusion, bloom, and support for institutional light fixtures (area lights, tube lights). All work must stay within OpenGL 4.1 Core Profile constraints.

</domain>

<decisions>
## Implementation Decisions

### Shadow quality
- **D-01:** Shadows should be soft and diffuse — PCF with large kernel or PCSS (Percentage-Closer Soft Shadows). Shadows fade smoothly at edges, matching Stanley Parable's gentle, non-dramatic lighting.
- **D-02:** Add cascaded shadow maps (CSM) for the sun/directional light so outdoor-facing windows cast light shafts. Keep spot shadows for key interior fixtures.
- **D-03:** Expand beyond the current 2 spot shadow limit — spot + directional shadow support. Point light shadows are NOT in scope.

### Ambient occlusion
- **D-04:** Subtle contact AO — gentle darkening where walls meet floors, around door frames, and under furniture. Not heavy-handed — just enough to ground objects and add depth. Clean, well-lit institutional feel.
- **D-05:** SSAO technique is Claude's discretion — research should determine best technique for OpenGL 4.1 (classic SSAO, HBAO, GTAO, etc.) based on quality/performance tradeoffs.

### Bloom & glow
- **D-06:** Soft warm glow via multi-pass Gaussian downsample/upsample chain (like Unreal's bloom). Dreamy, warm halo around light sources and bright surfaces — the Stanley Parable signature where overhead lights gently bleed into their surroundings.
- **D-07:** Replace the current crude 8-sample bloom tap with a proper mip-chain bloom pipeline.
- **D-08:** Bloom is configurable per-environment — different rooms can have different bloom intensity/threshold. The per-environment post-process params system already exists.

### Light types & shapes
- **D-09:** Add rectangular area lights — recessed ceiling panels, fluorescent fixtures, windows. Area lights produce naturally soft illumination with realistic falloff. This is the key light type for institutional settings.
- **D-10:** Research and implement tube/line lights for corridor fluorescent fixtures if feasible within OpenGL 4.1.
- **D-11:** Research emissive mesh lighting (letting mesh surfaces emit light) — implement if achievable with good visual return.
- **D-12:** Claude has discretion on which new light types to actually implement based on what's achievable in OpenGL 4.1 with good quality/performance return.

### Light units
- **D-13:** Keep arbitrary intensity values (current float system). No physically-based units — the art-directed style benefits more from hand-tuning than from physical accuracy.

### Claude's Discretion
- SSAO technique selection (classic SSAO vs HBAO vs GTAO)
- Shadow map resolution and cascade count for CSM
- Bloom mip chain depth and threshold tuning
- Which of the new light types (area, tube, emissive) to actually implement based on research findings
- Soft shadow technique (PCF kernel size vs PCSS)
- Any performance optimizations needed to maintain frame rate with new features
- AO sample count, radius, and blur approach

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Current rendering pipeline
- `assets/shaders/game/scene.frag` — Main PBR lighting shader: Cook-Torrance BRDF, quartic attenuation, 32-light loop, spot/point/directional, hemisphere ambient, shadow sampling
- `assets/shaders/engine/composite.frag` — Post-process pipeline: bloom (current 8-tap), tonemapping (ACES), fog, vignette, split toning, sky rendering
- `src/engine/rendering/lighting/RenderLight.h` — Light data structures: RenderLight, LightingEnvironment, DirectionalLightSlot, LightType enum
- `src/engine/rendering/lighting/ShadowMap.h` — Current shadow map implementation (single 2D depth texture per map)
- `src/game/rendering/RuntimeSceneRenderer.h` — Scene render orchestration: shadow pass, scene pass, post-process pass
- `src/game/components/LightComponent.h` — ECS light component (type, color, radius, intensity, direction, cone angles, castsShadows)

### Environment system
- `src/game/rendering/EnvironmentDefinition.h` — Environment definition with post-process params, sky settings, lighting environment
- `assets/defs/environments/default.environment` — Current default environment settings (exposure, bloom, AO, fog, etc.)
- `src/engine/rendering/post/PostProcessParams.h` — Post-process parameter struct consumed by composite pass

### Project context
- `.planning/PROJECT.md` — Core value: Stanley Parable aesthetic, OpenGL 4.1 constraint
- `.planning/ROADMAP.md` — Phase 4 goal and dependencies

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **PBR lighting loop** (`scene.frag:791-854`): Cook-Torrance BRDF with GGX, Smith geometry, Schlick Fresnel — solid foundation to extend with new light types
- **Attenuation function** (`scene.frag:73-78`): Quartic windowed inverse-square — keep as-is for point/spot, may need adaptation for area lights
- **Shadow infrastructure** (`ShadowMap.h`, `RuntimeSceneRenderer`): FBO-based depth shadow maps with PCF sampling — extend for CSM and additional shadow slots
- **Post-process pipeline** (`CompositePass`, `composite.frag`): Fullscreen quad with configurable effects — bloom replacement fits naturally here
- **Environment system** (`EnvironmentDefinition`, `.environment` files): Per-scene post-process and lighting params — new AO/bloom params integrate here

### Established Patterns
- **Shader uniforms**: All lighting params passed as uniforms from C++ to GLSL — new light types follow this pattern
- **Light array**: `uLights[32]` struct array with type dispatch in shader — area/tube lights extend the RenderLight struct and LightType enum
- **Post-process chain**: Scene FBO → composite FBO → stylize → screen — SSAO and bloom passes insert into this chain
- **Per-environment config**: `.environment` text files with key-value pairs — new params (AO strength, bloom mip count) follow this format

### Integration Points
- **New light types**: Extend `LightType` enum, `RenderLight` struct, `LightComponent`, scene.frag lighting loop
- **SSAO pass**: New FBO + shader between scene pass and composite pass; AO texture sampled in scene.frag or composite.frag
- **Bloom replacement**: Replace `bloomGlow()` in composite.frag with multi-pass downsample/upsample chain; may need intermediate FBOs
- **CSM**: Multiple shadow map textures for directional light; shadow matrix array in scene.frag; cascade selection by fragment depth

</code_context>

<specifics>
## Specific Ideas

- The Stanley Parable's overhead lights have a characteristic warm glow that bleeds into the ceiling and surrounding walls — this comes from proper bloom + area light shape
- Institutional fluorescent lighting (recessed panels, tube lights) is the defining light fixture of the prison setting
- Shadows should be soft enough that they "suggest" geometry rather than drawing hard lines — matching the clean, minimalist aesthetic
- AO should be barely noticeable until you toggle it off — it provides depth without drawing attention to itself

</specifics>

<deferred>
## Deferred Ideas

- Point light shadow maps (cubemap shadows) — expensive, could be a future optimization phase
- Physically-based light units (lumens/lux) — not needed for art-directed style
- Light probes / IBL (image-based lighting) — would improve ambient quality further but is a separate feature
- Screen-space reflections — related to lighting quality but distinct scope
- Volumetric lighting / god rays — impressive but separate scope

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting*
*Context gathered: 2026-03-30*
