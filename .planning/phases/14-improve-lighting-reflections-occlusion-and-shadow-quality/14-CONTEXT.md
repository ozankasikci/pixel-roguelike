# Phase 14: Improve Lighting, Reflections, Occlusion, and Shadow Quality - Context

**Gathered:** 2026-04-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Upgrade the current renderer from a strong direct-lighting foundation into a polished, game-focused lighting stack: believable environment reflections, subtler but higher-quality occlusion, more stable shadows, and practical performance controls for editor play preview. The target is "looks good by default" for this game's stylized institutional scenes, not a feature-complete Unreal/Unity lighting clone.

</domain>

<decisions>
## Implementation Decisions

### Visual Direction
- **D-01:** Prioritize a small, opinionated lighting stack that looks good in the shipped game. Do not expand scope toward a full "industry-standard engine" feature matrix.
- **D-02:** The main missing visual feature is believable environment reflection. The current strange "reflection" look on doors is not acceptable as the long-term result.
- **D-03:** Preserve the established art direction: warm soft lighting, clean geometry, muted colors, stylized realism.

### Reflections
- **D-04:** Implement real surface reflections from the environment, not just stronger direct-light highlights.
- **D-05:** Start with global sky-driven specular IBL using the existing sky cubemap system.
- **D-06:** If global sky reflections are still not sufficient indoors, add limited local reflection probes with box projection for rectangular rooms.
- **D-07:** Reflection probes should be practical, not fully dynamic-engine-general: update-once/local use is preferred over expensive always-updating probes.
- **D-08:** Screen-space reflections are not the primary solution for this phase. They may be deferred entirely if the global + probe path delivers the needed look.

### Occlusion
- **D-09:** Keep screen-space ambient occlusion, but make it subtler, cleaner, and more performant. AO should ground objects without drawing attention to itself.
- **D-10:** AO remains screen-space only for this phase. No full GI, voxel GI, or probe-based indirect diffuse lighting.

### Shadows
- **D-11:** Keep the current shadow scope centered on the sun CSM and selected local shadow casters. Improve quality and stability rather than adding every shadow type.
- **D-12:** Point-light cubemap shadows, area-light shadows, and tube-light shadows are out of scope for this phase.
- **D-13:** Prioritize more stable directional shadows, better filtering, and cleaner bias/cascade behavior over adding new shadow classes.

### Performance
- **D-14:** The current fullscreen editor play-preview performance is too low for simple scenes. Around mid-40 FPS in the initial scene is not acceptable.
- **D-15:** Add explicit quality/performance controls for editor play preview rather than forcing one fixed full-quality path at all times.
- **D-16:** Render-scale support and preview-quality presets are in scope for this phase.
- **D-17:** Existing pass timing stats should remain, and the new system should make it easier to see where cost is going.

### Editor/Authoring
- **D-18:** Lighting/reflection improvements must work in the runtime and in the editor play preview.
- **D-19:** Reflection and quality controls should be exposed through existing environment/editor surfaces rather than a separate one-off debug UI.
- **D-20:** Keep authoring practical for this project. If probes are added, the scene/editor format should support them directly.

### Explicitly Out of Scope
- **D-21:** No full GI solution (Lumen, baked GI overhaul, DDGI, probe volumes, voxel GI).
- **D-22:** No physically based light units, Kelvin authoring, or IES profiles.
- **D-23:** No volumetric fog / volumetric lighting pass in this phase.
- **D-24:** No renderer backend migration (Metal/Vulkan) in this phase.

### Claude's Discretion
- Exact OpenGL 4.1 implementation details for specular IBL prefiltering and BRDF LUT generation
- Whether local probes should be runtime-captured, asset-backed, or hybrid
- Exact SSAO optimization path (half-res, blur strategy, fade rules, upsample behavior)
- Exact preview quality preset values and default render scale
- Exact test coverage split between rendering/unit/scene roundtrip/editor checks

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Current render pipeline
- `src/engine/rendering/SceneRenderPipeline.h` — shared runtime/editor/model-viewer pipeline orchestration
- `src/engine/rendering/SceneRenderPipeline.cpp` — current pass order, shadow submission, and post-processing flow
- `src/engine/rendering/core/Framebuffer.cpp` — current MRT/depth allocation; key fill-rate/perf cost
- `src/engine/rendering/post/SsaoPass.h` — current SSAO API
- `src/engine/rendering/post/SsaoPass.cpp` — current 32-sample full-resolution SSAO + blur implementation
- `src/engine/rendering/post/CompositePass.cpp` — sky/background compositing and final post stack wiring
- `src/engine/rendering/post/SkyTextureLibrary.h` — existing sky cubemap/panorama loading cache
- `src/engine/rendering/lighting/RenderLight.h` — current light types, lighting environment, and shadow limits

### Shaders
- `assets/shaders/game/scene.frag` — current direct-light-only material shading; no environment specular reflection path yet
- `assets/shaders/engine/composite.frag` — current sky cubemap/panorama usage for background rendering only
- `assets/shaders/engine/ssao.frag` — current AO kernel sampling path
- `assets/shaders/engine/ssao_blur.frag` — current blur strategy

### Environment/editor authoring
- `src/engine/rendering/post/PostProcessParams.h` — AO, bloom, fog, and debug controls
- `src/engine/rendering/post/SkySettings.h` — current sky/panorama/cubemap settings
- `src/game/rendering/EnvironmentDefinition.h` — serialized environment definition shape
- `src/editor/ui/EditorEnvironmentPanel.cpp` — environment control surface in the editor
- `apps/level_editor/main.cpp` — preview rendering path, fullscreen preview behavior, and viewport stats overlay

### Prior lighting work
- `.planning/phases/04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting/04-CONTEXT.md` — previous lighting-phase decisions and deferrals
- `.planning/phases/04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting/04-RESEARCH.md` — prior rendering research and pitfalls
- `.planning/phases/05-unify-editor-runtime-build-rendering-parity/05-CONTEXT.md` — render parity decisions relevant to editor/runtime consistency

### Scene content under discussion
- `assets/scenes/initial_scene.scene` — current first scene with visible door-highlight issue and fullscreen editor-play performance complaint
- `assets/scenes/silos_cloister.scene` — large daylight cloister scene for outdoor/interior shadow and reflection checks
- `assets/defs/environments/default.environment` — common environment baseline
- `assets/defs/environments/outdoor_bright.environment` — sky-cubemap-driven outdoor environment reference

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SkyTextureLibrary` already loads cubemap faces for the sky background, so reflection work can reuse real project sky assets instead of inventing a separate asset pipeline first.
- `SceneRenderPipeline::render()` already distinguishes `internalWidth/internalHeight` from `outputWidth/outputHeight`, which means render-scale support exists architecturally even though the editor currently feeds the same values for both.
- The renderer already has pass timing stats (`shadow`, `scene`, `ssao`, `bloom`, `composite`) visible in the editor overlay, which gives this phase an immediate verification surface.
- `SsaoPass` already has a dedicated FBO path and could be optimized without rearchitecting the whole renderer.

### Current Gaps
- `scene.frag` computes ambient + direct lighting only; it does not sample the sky cubemap or any reflection probe for surface specular.
- `composite.frag` does sample the sky cubemap, but only for the background/sky, not for material shading.
- The current framebuffer is expensive: `GL_RGB16F + GL_RGBA16F + GL_RGBA16F + depth` at full internal resolution.
- SSAO currently runs at full resolution with a 32-sample kernel and full-resolution bilateral blur.
- Fullscreen editor play preview currently uses output resolution as internal render resolution, so high-DPI fullscreen preview amplifies fill-rate cost immediately.

### Practical Fit for This Phase
- The engine already has enough sky, material, and post-processing infrastructure to add specular IBL without building a full GI system.
- Reflection probes are a reasonable second step because the game has rectangular indoor spaces where box projection will matter.
- Shadow improvements should focus on stability and tuning, not new light classes.

</code_context>

<specifics>
## Specific Ideas

- The current "reflection" people notice on the wooden/painted doors is likely just glossy direct specular plus bloom, not a real environmental reflection. This phase should make that distinction disappear by giving surfaces a real reflected environment source.
- Initial/default scenes should become the visual acceptance surfaces for this phase, not just synthetic render tests.
- Fullscreen preview performance needs a practical escape hatch. Quality presets and render scale are acceptable as long as the default experience still looks strong.

</specifics>

<deferred>
## Deferred Ideas

- Full GI or probe-volume diffuse lighting
- Screen-space reflections as a primary reflection technique
- Point-light shadow cubemaps
- Volumetric fog/light shafts
- Physical light units / Kelvin / IES
- Metal/Vulkan backend work

</deferred>

---

*Phase: 14-improve-lighting-reflections-occlusion-and-shadow-quality*
*Context gathered: 2026-04-03*
