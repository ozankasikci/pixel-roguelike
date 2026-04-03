# Phase 14: Improve Lighting, Reflections, Occlusion, and Shadow Quality - Research

**Researched:** 2026-04-03
**Domain:** Environment reflections, reflection probes, SSAO/shadow quality, and editor-preview performance for an OpenGL 4.1 custom renderer
**Confidence:** HIGH on renderer diagnosis and feature gaps; MEDIUM-HIGH on exact implementation sizing because local reflection probes touch both runtime and editor content paths

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** This phase should produce a small, good-looking lighting stack for this game, not a feature-complete engine-lighting system.
- **D-04:** Real surface reflections are required; current "fake reflection" highlights are not enough.
- **D-05:** Global sky-driven specular IBL is the first reflection step.
- **D-06:** Local reflection probes are acceptable if needed for indoor correctness.
- **D-09:** Keep AO screen-space and subtle.
- **D-11:** Improve existing shadows rather than adding all shadow types.
- **D-14:** Fullscreen editor preview performance must improve materially from the current rough ~45 FPS complaint.
- **D-16:** Quality presets and render scale are in scope.

### Deferred by Decision
- No full GI
- No volumetrics
- No physical light units
- No point-light cubemap shadows
- No renderer-backend rewrite

</user_constraints>

---

## Summary

The renderer already has a strong direct-lighting base: Cook-Torrance shading, sun CSM, local shadows, bloom, SSAO, stylize, sky cubemaps, and a shared scene pipeline. The biggest missing piece is that **surface materials do not currently receive environment specular lighting at all**. The sky cubemap exists, but only the background/post stack uses it.

That creates two visible problems:

1. **Materials look less grounded than they should.** Glossy doors, metal, polished stone, and wet surfaces only react to explicit light sources, so they can read as "plastic highlight" instead of "reflecting the world."
2. **The current shiny look is hard to reason about.** Users see something that feels like a reflection, but it is really direct specular + bloom rather than a captured environment.

The practical recommendation for this engine is:

1. **Add global specular IBL from the existing sky cubemap first.**
2. **Add a small local reflection-probe system with box projection for indoor rooms.**
3. **Improve SSAO and shadow quality while adding render-scale/quality controls for fullscreen preview performance.**

This gets the project much closer to "good-looking modern stylized lighting" without chasing a full GI stack.

---

## Current Renderer Diagnosis

### 1. Reflections are not real environment reflections yet

Local code review shows:

- `assets/shaders/game/scene.frag` computes ambient + direct-light shading and shadowing, but it does not sample a sky cubemap or reflection probe for material specular.
- `assets/shaders/engine/composite.frag` does sample `uSkyCubemap`, but only for background/sky rendering.
- `src/engine/rendering/post/SkyTextureLibrary.h` already loads sky cubemaps, so the asset side exists.

**Conclusion:** The renderer already has sky cubemap assets, but not a material-side reflection path. The first reflection improvement should reuse this existing cubemap infrastructure.

### 2. Current SSAO is visually serviceable but expensive and not scalability-aware

Local code review shows:

- `src/engine/rendering/post/SsaoPass.cpp` uses a 32-sample kernel.
- SSAO currently allocates FBOs at the full internal resolution.
- The blur pass is also full-resolution.
- The editor preview currently runs fullscreen preview at full internal resolution as well.

**Conclusion:** The AO implementation is a good base for subtle contact darkening, but it should be moved toward half-res + depth-aware upscale/blur + fade controls for better performance and cleaner large-scene behavior.

### 3. Fullscreen preview is likely fill-rate bound

Local code review shows:

- `src/engine/rendering/core/Framebuffer.cpp` allocates a fairly heavy MRT/depth setup.
- `apps/level_editor/main.cpp` feeds the same `targetW/targetH` for both internal and output resolution in fullscreen preview.
- `SceneRenderPipeline` already supports separate internal vs output sizes, but the editor doesn’t use that lever.

This matches Khronos’s OpenGL performance guidance: if performance changes significantly with render size, the renderer is at least partly fragment/fill-rate bound. [OpenGL Wiki: Performance](https://wikis.khronos.org/opengl/performance)

**Conclusion:** Preview render scale is a high-ROI, low-risk improvement for this phase.

### 4. Shadows are capable, but still a targeted system

Local code review shows:

- `RenderLight` supports point, spot, directional, rect, and tube lights.
- Only selected local lights receive shadow slots; shadowed spots are capped.
- Sun shadows use a 3-cascade CSM path.
- The system is already good enough to improve, rather than replace.

**Conclusion:** Phase 14 should tune and stabilize the current shadowing model instead of trying to add a totally new one.

---

## External Guidance

### A. Global reflections from sky/environment are standard and expected

Unity’s Lighting window explicitly separates **Environment Reflections** from direct light and allows the global reflection source to come from the **Skybox** or a **custom cubemap**. [Unity Lighting Window](https://docs.unity3d.com/es/2019.4/Manual/lighting-window.html)

Unreal’s Sky Light system exists to capture distant scene/sky lighting and apply it to the scene as environment lighting and reflections. [Unreal Sky Lights](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine?application_version=5.6)

**Implication for this engine:** The current sky cubemap should not remain background-only. It should become the global reflection source for materials.

### B. Local reflection probes are the right indoor follow-up

Godot’s `ReflectionProbe` docs are especially useful here because they match the scale of this project better than Lumen-style systems:

- `UPDATE_ONCE` is recommended for most objects/scenes.
- `UPDATE_ALWAYS` has significant performance cost and should be used sparingly.
- `box_projection` is specifically recommended to make reflections look more correct in rectangular rooms. [Godot ReflectionProbe](https://docs.godotengine.org/en/latest/classes/class_reflectionprobe.html)

Unity also treats reflection probes as the local reflection solution layered on top of global environment reflections. [Unity Reflection Probes](https://docs.unity3d.com/Manual/class-ReflectionProbe.html)

**Implication for this engine:** A small set of box-projected local probes with update-once capture is the right interior-reflection solution. It is enough for rooms like `initial_scene` without requiring full dynamic GI.

### C. Screen-space AO should stay subtle and scalable

Godot’s environment/post-processing guidance emphasizes that SSAO is a view-dependent depth cue and should be tuned conservatively. It also exposes quality/performance settings rather than one fixed implementation path. [Godot Environment and Post-Processing](https://docs.godotengine.org/en/stable/tutorials/3d/environment_and_post_processing.html)

Unity likewise treats SSAO as a post effect layered into a broader lighting stack, not as a replacement for lighting or GI. [Unity SSAO](https://docs.unity3d.com/Manual/PostProcessing-AmbientOcclusion.html)

**Implication for this engine:** Keep SSAO, but give it distance fade, quality presets, and reduced-resolution modes.

### D. Better engines separate visual quality from viewport cost

Unity and other modern tools expose dynamic resolution or equivalent quality levers for keeping preview/play responsive. [Unity Dynamic Resolution](https://docs.unity3d.com/Manual/DynamicResolution.html)

The OpenGL Wiki recommends using window-size changes as an immediate diagnostic for fragment/fill-rate bottlenecks. [OpenGL Wiki: Performance](https://wikis.khronos.org/opengl/performance)

**Implication for this engine:** Editor play preview should expose render scale and quality presets instead of assuming the only "correct" path is full-resolution/full-quality at all times.

---

## What We Lack Compared to a Strong "Looks Good" Lighting Stack

Not compared to every Unreal feature, but compared to the minimum set that typically makes scenes feel convincingly polished:

1. **No real environment specular lighting on materials**
2. **No local reflection-probe system for interiors**
3. **No performance-scalable AO pipeline**
4. **No editor-facing preview quality ladder**
5. **Limited shadow quality/stability tuning for large fullscreen preview cases**

These are all achievable without adding a full GI system.

---

## Recommended Phase Architecture

### Recommendation 1: Global sky-driven specular IBL first

Use the existing sky cubemap faces as the source of a prefiltered specular environment map and BRDF LUT for the material shader.

Why first:
- It closes the biggest visual gap.
- It reuses existing sky assets.
- It makes reflections explainable and intentional.
- It improves metals, polished surfaces, wet surfaces, and glossy painted wood immediately.

### Recommendation 2: Box-projected local reflection probes second

After global IBL is in, add a limited local probe path:
- update-once capture or asset-backed cubemap assignment
- box projection
- blend distance
- indoor focus

Why second:
- Interior rooms are where global sky-only reflections break down fastest.
- This is the smallest local-reflection system that meaningfully improves the game’s spaces.

### Recommendation 3: AO + shadows + preview quality as one optimization/polish pass

Bundle these together because they all affect the same fullscreen preview cost and user perception:
- half-res / better-scaled SSAO
- CSM stability/filtering/bias improvements
- render scale and preview presets
- keep per-pass stats visible

Why together:
- They share the same verification surface: fullscreen editor play preview.
- They are all part of the current performance complaint.

---

## Recommended Plan Breakdown

### Plan 01
Add global specular IBL using the existing sky cubemap path and bind it into `scene.frag`.

### Plan 02
Add local reflection probes with box projection and authoring/serialization support for indoor scenes.

### Plan 03
Improve AO/shadow quality and add editor preview quality/render-scale controls so the new reflection system remains usable at speed.

---

## Risks and Mitigations

### Risk: Local probes become too engine-general
Mitigation: keep them limited to update-once/local room use; no probe volumes, no full GI blending graph.

### Risk: Runtime cubemap prefiltering is too expensive
Mitigation: cache generated specular cubemaps aggressively; use modest probe resolution and update-once policy.

### Risk: AO and new reflections worsen fullscreen performance further
Mitigation: pair quality work with render scale + AO scalability in the same phase rather than after the fact.

### Risk: Reflection controls end up disconnected from scene authoring
Mitigation: expose probe/environment controls in existing scene/environment/editor surfaces, not a hidden debug path.

---

## Final Recommendation

Do **not** build GI first.

The highest-value path for this game is:

1. real sky-driven specular reflections,
2. a small local reflection-probe system for indoor rooms,
3. cleaner SSAO and more stable shadows,
4. explicit preview quality controls so the improved lighting still feels fast to iterate on.

That is the smallest coherent lighting upgrade that will materially improve both look and usability.

---

## Sources

- [Unity Lighting Window](https://docs.unity3d.com/es/2019.4/Manual/lighting-window.html)
- [Unity Reflection Probes](https://docs.unity3d.com/Manual/class-ReflectionProbe.html)
- [Unity Dynamic Resolution](https://docs.unity3d.com/Manual/DynamicResolution.html)
- [Unity SSAO](https://docs.unity3d.com/Manual/PostProcessing-AmbientOcclusion.html)
- [Unreal Sky Lights](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine?application_version=5.6)
- [Godot ReflectionProbe](https://docs.godotengine.org/en/latest/classes/class_reflectionprobe.html)
- [Godot Environment and Post-Processing](https://docs.godotengine.org/en/stable/tutorials/3d/environment_and_post_processing.html)
- [OpenGL Wiki: Performance](https://wikis.khronos.org/opengl/performance)
