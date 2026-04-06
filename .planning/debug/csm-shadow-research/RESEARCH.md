# CSM Shadow Junction Artifacts - Research

**Researched:** 2026-04-06
**Domain:** Shadow mapping, cascaded shadow maps, bias techniques, geometry junction artifacts
**Confidence:** HIGH (extensively documented problem with well-established solutions)

## Summary

Shadow seam artifacts at wall-floor and wall-ceiling junctions are a fundamental geometric problem caused by finite shadow map resolution combined with the discontinuous surface normals at concave geometry junctions. The artifact is NOT caused by a bug -- it is an inherent limitation of shadow mapping that every professional engine addresses through a combination of techniques.

The current engine uses a fragile dual-mechanism approach (caster-side offset + receiver-side normal offset) that works but is architecturally brittle. The research identifies that the **receiver-side normal offset** technique (based on Daniel Holbert's GDC 2011 "Saying Goodbye to Shadow Acne") is the industry-standard solution, but the engine's implementation has a specific weakness: it cannot handle wall-ceiling junctions because the wall surface's horizontal normal pushes the shadow lookup sideways rather than toward the ceiling.

The recommended approach is a **three-layer defense** that eliminates the need for caster-side geometry manipulation entirely:
1. Proper receiver-side normal offset with per-cascade texel scaling
2. Screen-space contact shadows as a post-process gap filler
3. GL_DEPTH_CLAMP during shadow map rendering to maximize depth precision

**Primary recommendation:** Replace the caster-side offset with screen-space contact shadows (ray-marched in the depth buffer), which fill junction gaps from the camera's perspective regardless of surface normal orientation. Combine with a properly-scaled receiver-side normal offset and GL_DEPTH_CLAMP.

---

## Part 1: Root Cause Analysis

### Why the Artifact Happens

The bright line at geometry junctions (wall-floor, wall-ceiling, wall-wall) is caused by a **shadow coverage gap** -- a region of space where no shadow map texel records an occluder, even though the geometry should be in shadow.

#### The Geometric Cause

Consider a wall meeting a floor at a 90-degree junction, with a directional light coming from above-left:

```
    Light Direction
        \  \  \
         \  \  \
    ------+------  <-- Ceiling
    |     |     |
    |Wall |     |
    |     |     |
    ------+------  <-- Floor
```

**At the wall-floor junction:**
- The shadow map is rendered from the light's perspective
- The wall's bottom edge and the floor surface meet at a point
- In the shadow map, the wall writes depth values for its own surface
- The floor writes its own depth values
- At the exact junction point, the wall's bottom-most texel in the shadow map may not extend all the way to the floor due to:
  1. **Finite texel resolution**: The shadow map texel grid does not perfectly align with the geometry edge
  2. **Depth quantization**: The wall edge's depth is stored at the center of its texel, leaving a sub-texel gap
  3. **Orthographic projection discretization**: The light's orthographic projection snaps to texel boundaries, potentially leaving a thin strip uncovered

**At the wall-ceiling junction (grazing angle):**
- The wall's top edge is nearly perpendicular to the light direction
- At grazing angles, a single shadow map texel spans a large world-space distance along the wall
- The wall's top edge may not produce enough shadow coverage to reach the ceiling
- This is the "projective aliasing" problem -- surfaces nearly parallel to the light direction have extremely poor shadow map coverage

#### Why It Appears as a Bright Line

When the shadow lookup for a floor pixel near the wall finds no occluder in the shadow map (the gap), it returns "fully lit" (visibility = 1.0). This creates a thin bright strip exactly at the junction. The width of the strip is proportional to the shadow map texel size in world space.

#### Mathematical Formulation

For a cascade covering distance `D` with resolution `R`:
- World-space texel size = `D / R`
- For a 1024-resolution shadow map covering 20 meters: texel = ~0.02m (2cm)
- For a cascade covering 80 meters: texel = ~0.08m (8cm)

The gap width at a junction equals approximately one shadow map texel in world space, projected onto the receiving surface. For grazing angles, this can be much larger due to projective aliasing:
- Gap width = `texel_size / sin(angle_between_surface_and_light)`
- At 10-degree grazing angle: gap = `texel_size / sin(10) = texel_size * 5.76`

This is why the artifact is much worse at wall-ceiling junctions (grazing angle) than wall-floor junctions (more direct angle).

### Why the Current Fix is Fragile

The current dual-mechanism approach has a specific architectural weakness:

**Mechanism 1 (caster offset)** moves ALL shadow geometry toward the light. This is a global perturbation -- it affects every shadow in the scene, not just the junction gaps. The 0.15 unit offset (currently set in SceneRenderPipeline.cpp, despite comments saying 0.06) shifts the entire wall's shadow map coverage upward, which:
- Closes the wall-ceiling gap (good)
- Opens a gap at the wall-floor junction (bad -- the floor triangle artifact)
- Creates slight shadow offset for all other geometry (acceptable but not free)

**Mechanism 2 (receiver normal offset)** shifts the shadow lookup along the surface normal. This works for floor/ceiling receivers (vertical normal pushes lookup into wall's shadow) but fails for wall receivers at ceiling junctions (horizontal normal pushes lookup sideways, not toward the gap).

The two mechanisms compensate for each other's weaknesses, but the compensation is parameter-sensitive. The values 0.15 (caster) and 0.15 (receiver) are balanced against each other -- changing either one regresses the other's fix.

---

## Part 2: Professional Engine Approaches

### Unreal Engine (UE4 CSM / UE5 Virtual Shadow Maps)

**UE4 Cascaded Shadow Maps:**
- Uses a combination of constant bias + slope-scaled bias + normal offset bias
- Normal offset scales per cascade (larger cascades get proportionally larger offsets)
- Bias values are exposed as per-light properties, not hardcoded
- Front-face culling during shadow map rendering is optional per project

**UE5 Virtual Shadow Maps (VSM):**
- Fundamentally different approach: virtualized shadow map pages with extremely high effective resolution
- Eliminates most bias artifacts through sheer resolution (16K equivalent near camera)
- Tightly integrated with Nanite mesh rendering
- Not applicable to our OpenGL 4.1 engine, but demonstrates the principle: higher resolution reduces junction gaps

**Key insight from UE:** Normal offset bias should scale with cascade texel size, not be a fixed world-space value.

### Unity

- Uses slope-based normal offset bias as the primary anti-acne mechanism
- Shadow bias and normal bias are per-light parameters
- Uses `GL_DEPTH_CLAMP` (or equivalent) during shadow map rendering
- Screen-space shadows available as a post-process effect in URP/HDRP to fill contact gaps

### id Tech (Doom 2016, Doom Eternal)

- Doom 2016 uses virtual texturing for shadow maps (similar concept to UE5 VSM)
- id Tech 7 uses "mega-texture" shadow approach with very high resolution near camera
- For conventional shadow maps in earlier id Tech: aggressive PCF filtering + back-face rendering

### Godot

- Uses normal offset bias as primary mechanism
- Godot PR #51025 simplified bias to: normalized cascade-size-based scaling
- Normal bias in Godot is actually slope-scaled (proportional to `1 - dot(N, L)`)
- Known issues with Peter Panning when bias values are too large

### The Witness (Ignacio Castano)

- Tested all four bias types: constant, slope-scale, normal offset, receiver plane depth bias
- Disabled receiver plane depth bias due to degenerate cases with hardware texture filtering
- Used combination of normal offset + slope-scale bias
- Blended bias magnitude between cascades to prevent visible offset discontinuity at cascade boundaries
- This is one of the most thoroughly documented practical implementations

---

## Part 3: Industry-Standard Techniques (Ranked for OpenGL 4.1)

### Tier 1: Primary Defense (MUST implement)

#### 1. Receiver-Side Normal Offset with Per-Cascade Texel Scaling

**Confidence:** HIGH (used by UE4, Unity, Godot, The Witness, DigitalRune)

This is the single most important technique. The current engine already has this (Mechanism 2 in scene.frag) but the implementation has two issues:

**Issue A: Fixed world-space offset.** The current code uses `N * 0.15 * slopeFactor` -- a fixed 0.15 world-space units. This should scale with the cascade's texel size so that the offset is always proportional to shadow map resolution:

```glsl
// Current (fragile):
vec3 biasedWorldPos = fragWorldPos + N * 0.15 * csmSlopeFactor;

// Improved (scales with cascade):
float cascadeTexelSize = uCsmSplitDistances[layer] / float(textureSize(uCsmShadowMap, 0).x);
float normalOffsetScale = 2.0;  // number of texels to offset (tunable, 1-3 range)
vec3 biasedWorldPos = fragWorldPos + N * cascadeTexelSize * normalOffsetScale * csmSlopeFactor;
```

With texel-based scaling:
- Cascade 0 (20m range, 1024 res): offset = 2 * 0.02m * slope = up to 0.04m
- Cascade 1 (80m range, 1024 res): offset = 2 * 0.08m * slope = up to 0.16m
- Cascade 2 (400m range, 1024 res): offset = 2 * 0.39m * slope = up to 0.78m

**Issue B: Cannot fix wall surfaces at ceiling junctions.** When the wall's normal is horizontal and the gap is vertical, offsetting along the normal moves the lookup sideways. This is the fundamental weakness that Mechanism 1 (caster offset) currently compensates for. The solution is NOT a better normal offset -- it is a complementary technique that works from the camera's perspective (see screen-space contact shadows below).

#### 2. Proper Depth Bias (Slope-Scaled + Constant)

**Confidence:** HIGH (universal technique)

The current implementation uses `glPolygonOffset(1.1f, 4.0f)` during CSM rendering, plus a small shader-side bias. This is reasonable. The key improvement:

```glsl
// Depth-space bias should also scale per cascade
float constantBias = 0.0005;
float slopeBias = 0.002 * (1.0 - max(dot(N, L), 0.0));
float bias = max(constantBias, slopeBias);
// No need to divide by split distance -- the bias operates in normalized depth space
```

The current division by `uCsmSplitDistances[layer]` is unusual and may be compensating for an incorrect depth range. Verify that the cascade's orthographic projection produces depth values in [0, 1] range.

#### 3. GL_DEPTH_CLAMP During Shadow Map Rendering

**Confidence:** HIGH (OpenGL 3.2+ core feature, fully available in 4.1)

Shadow pancaking via `GL_DEPTH_CLAMP` prevents near-plane clipping of shadow casters that are outside the cascade frustum but should still cast shadows. This improves depth precision by allowing tighter near/far planes:

```cpp
// In SceneRenderPipeline::renderShadows, CSM loop:
glEnable(GL_DEPTH_CLAMP);
for (int cascade = 0; cascade < CascadedShadowMap::kCascadeCount; ++cascade) {
    // ... render cascade ...
}
glDisable(GL_DEPTH_CLAMP);
```

With depth clamping, geometry in front of the near plane gets clamped to depth 0 rather than being clipped. This means the near plane can be placed at the view frustum boundary without worrying about missing casters behind it. The tighter depth range gives more precision, reducing the quantization gap at junctions.

**Current engine issue:** The `buildCascadeMatrix` function uses `zMult = 10.0` to pull the near plane back. With GL_DEPTH_CLAMP, this multiplier can be reduced to 1.0-2.0, significantly improving depth precision.

### Tier 2: Gap Elimination (RECOMMENDED)

#### 4. Screen-Space Contact Shadows

**Confidence:** HIGH for technique, MEDIUM for specific implementation

This is the technique that eliminates the need for the caster-side offset entirely. Screen-space contact shadows work by ray-marching in the depth buffer from each pixel toward the light. They detect occlusion from the camera's perspective, which means:

- Junction gaps that exist in the shadow map are invisible to the depth buffer -- the depth buffer sees continuous geometry at the junction
- The technique naturally fills exactly the gaps that shadow maps miss
- It works regardless of surface normal orientation (solves the wall-ceiling problem)

**Implementation approach (OpenGL 4.1 compatible):**

```glsl
// In scene.frag, after CSM shadow sampling:
float contactShadow = 1.0;
if (uEnableContactShadows != 0) {
    // Ray march from fragment position toward light in screen space
    vec3 rayStart = vViewPos;  // fragment position in view space
    vec3 rayDir = normalize((uView * vec4(-uSunDirection, 0.0)).xyz);  // light dir in view space
    
    float stepSize = 0.05;  // world units per step (tune for your scale)
    int maxSteps = 16;
    float maxDistance = 0.5;  // max world-space distance to trace
    
    for (int i = 1; i <= maxSteps; i++) {
        vec3 samplePos = rayStart + rayDir * stepSize * float(i);
        vec4 sampleClip = uProjection * vec4(samplePos, 1.0);
        vec2 sampleUV = sampleClip.xy / sampleClip.w * 0.5 + 0.5;
        
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            break;
        
        float sceneDepth = texture(uDepthTexture, sampleUV).r;  // need depth buffer as texture
        float sampleDepth = sampleClip.z / sampleClip.w * 0.5 + 0.5;
        
        float thickness = 0.02;  // thickness threshold
        if (sampleDepth > sceneDepth && sampleDepth - sceneDepth < thickness) {
            contactShadow = 0.0;
            break;
        }
    }
    
    visibility = min(visibility, contactShadow);
}
```

**Cost:** 16 texture lookups per fragment, but only in the existing depth buffer (already in cache). Approximately 0.3-0.5ms at 1080p on modern GPUs. Can be limited to near-camera fragments only.

**Key advantage:** This technique fills gaps at geometry junctions from screen space, making it immune to shadow map resolution and bias parameter issues. It is the technique that would eliminate the caster-side offset entirely.

**Limitation:** Only works for geometry visible to the camera. Shadows behind the camera or around corners still rely on the shadow map. This is acceptable because junction gap artifacts are only visible where the camera can see them.

#### 5. Cascade Texel Snapping (Stable Cascades)

**Confidence:** HIGH (already partially implemented in the engine)

The engine's `buildCascadeMatrix` already snaps to texel-sized increments (lines 163-176 of CascadedShadowMap.cpp). This prevents shimmer but also helps with junction artifacts by ensuring consistent texel alignment frame-to-frame.

Verify the snapping is correct -- the current code snaps the center position but not the extents. The snapping should ensure the projection doesn't shift by sub-texel amounts when the camera moves.

### Tier 3: Quality Enhancement (OPTIONAL)

#### 6. Higher Shadow Map Resolution for Cascade 0

**Confidence:** HIGH (simple, direct improvement)

The current engine uses 1024 resolution for all cascades. Using 2048 for cascade 0 (nearest to camera, where junction artifacts are most visible) halves the texel size and thus halves the gap width. Memory cost: ~16MB additional for one 32-bit depth texture.

```cpp
// In CascadedShadowMap, use per-cascade resolution:
static constexpr int kResolutions[kCascadeCount] = {2048, 1024, 1024};
```

This requires changing the texture array to use a separate FBO per cascade (since GL_TEXTURE_2D_ARRAY requires uniform resolution) or using individual textures instead.

#### 7. PCF Filter Kernel Aware of Junctions

**Confidence:** MEDIUM (refinement of existing approach)

The current 16-tap Poisson disk PCF uses `min(centerVisibility, visibility)` which keeps edges sharp. For junction gaps specifically, the Poisson spread could be increased at junction pixels (detected via depth discontinuity) to blur over the gap:

```glsl
float depthGrad = fwidth(gl_FragCoord.z);
float junctionSpread = depthGrad > threshold ? 2.0 : 1.0;
float spread = 1.0 * junctionSpread;
```

This is a refinement, not a fix -- it blurs the artifact rather than eliminating it.

#### 8. Variance Shadow Maps (VSM) / Exponential Variance Shadow Maps (EVSM)

**Confidence:** MEDIUM (significant implementation effort, mixed results)

VSM/EVSM allow hardware texture filtering of the shadow map, which naturally softens junction gaps. However:
- **Light bleeding** at overlapping occluder depths (a major known problem)
- Requires 2-4 channel float texture instead of single-channel depth
- Memory cost: 4-8x more than standard depth shadow maps
- Does NOT eliminate junction gaps, only softens them

**Not recommended** as the primary solution. If soft shadows are desired for artistic reasons, EVSM could be considered, but screen-space contact shadows + normal offset bias achieves the same junction-gap fix with less memory and no light bleeding.

---

## Part 4: Is the Caster-Side Offset Fundamentally Wrong?

**Yes, it is a workaround, not a solution.** Here is why:

1. **Global perturbation:** Moving all shadow geometry toward the light affects every shadow in the scene. The "fix" for one junction type creates a regression for another junction type at different parameters.

2. **Not geometrically correct:** The shadow map should represent the actual geometry's occlusion. Moving geometry in the shadow map means the shadow map no longer represents reality -- it represents a fictional displaced scene.

3. **Parameter coupling:** The caster offset (0.15 units) and receiver normal offset (0.15 units with slope scaling) are balanced against each other. This coupling means any change to either value requires re-tuning the other.

4. **Scale dependence:** A fixed world-space offset (0.15 units) has different effects at different cascade scales. It might be perfect for cascade 0 but too small or too large for cascade 2.

The correct approach is to **render the shadow map from the actual geometry** (no caster offset) and then address gaps with receiver-side techniques that inherently scale with the shadow map's resolution.

---

## Part 5: Recommended Implementation Plan

### Phase 1: Remove Caster-Side Offset, Improve Receiver-Side

1. **Add GL_DEPTH_CLAMP** during CSM rendering
2. **Remove the caster-side offset** (set `uShadowCasterOffset` to 0)
3. **Scale normal offset per cascade texel size** instead of fixed world-space value
4. **Reduce zMult** in `buildCascadeMatrix` from 10.0 to 2.0 (depth clamp handles the rest)

Expected result: Wall-floor junctions fixed (normal offset handles them). Wall-ceiling junctions may still show artifacts (this is the gap that only screen-space shadows fix).

### Phase 2: Add Screen-Space Contact Shadows

1. **Bind the scene depth buffer as a texture** (you already render to FBO, so the depth texture is available)
2. **Implement contact shadow ray march** in scene.frag (16-step march, limited to 0.5 world units)
3. **Combine with CSM shadow:** `visibility = min(csmVisibility, contactShadow)`

Expected result: All junction gaps eliminated. The contact shadow fills exactly the sub-texel gaps that the shadow map misses.

### Phase 3: Cleanup

1. **Remove `uShadowCasterOffset` uniform** from shadow_depth.vert
2. **Remove `uLightDirection` uniform** from shadow_depth.vert (only needed for caster offset)
3. **Consolidate bias parameters** into a per-cascade struct
4. **Add ImGui debug controls** for normal offset scale and contact shadow parameters

### Why This Approach is Architecturally Robust

1. **No magic numbers in the shadow map pass:** The shadow map renders actual geometry with no offsets. Correct by construction.

2. **Receiver-side scaling is automatic:** Normal offset scales with cascade texel size. Adding cascades or changing resolution auto-adjusts.

3. **Screen-space contact shadows are independent:** They use only the existing depth buffer -- no coupling to shadow map parameters. They fill gaps from the camera's perspective, which is the only perspective that matters for visual correctness.

4. **No regression coupling:** Unlike the dual-mechanism approach where parameter A compensates for parameter B's weakness, each technique in this stack is independently correct for its domain:
   - Normal offset: handles self-shadowing and floor/ceiling junction gaps
   - Contact shadows: handles all visible junction gaps regardless of normal orientation
   - Depth clamp: maximizes depth precision, reducing quantization gaps

5. **Technique degradation is graceful:** Disabling contact shadows merely reveals the shadow map's native gaps (which are small with proper normal offset). Disabling normal offset only causes self-shadowing acne, not junction gaps (which contact shadows handle).

---

## Part 6: State of the Art (2024-2026)

| Old Approach | Current Approach | When Changed | Impact |
|---|---|---|---|
| Constant depth bias | Slope-scaled + normal offset | ~2011 (GDC) | Eliminated most shadow acne without Peter Panning |
| Fixed bias values | Per-cascade scaled bias | ~2014-2016 | Consistent quality across cascade boundaries |
| Standard shadow maps | Virtual Shadow Maps (UE5) | 2022 | Near-elimination of all bias artifacts through resolution |
| No contact shadows | Screen-space contact shadows | ~2018 (mainstream) | Gap-filling at geometry junctions and tiny occluders |
| Caster-side geometry offset | Receiver-side only + contact shadows | Industry consensus | No geometry perturbation in shadow pass |

**Key trend:** The industry has moved away from modifying shadow caster geometry and toward higher-resolution shadow maps (VSM in UE5) combined with screen-space supplementation. For engines that cannot use VSM-class approaches, the combination of proper receiver-side normal offset + screen-space contact shadows is the standard solution.

---

## Part 7: OpenGL 4.1 Compatibility Notes

All recommended techniques are fully compatible with OpenGL 4.1 Core Profile:

| Technique | GL Requirement | Available in 4.1? |
|---|---|---|
| GL_DEPTH_CLAMP | OpenGL 3.2 core | Yes |
| sampler2DArrayShadow (CSM) | OpenGL 4.0 core | Yes |
| dFdx/dFdy (receiver plane bias) | GLSL 1.10+ | Yes |
| Screen-space ray march | Fragment shader texture lookup | Yes |
| Poisson disk PCF | Fragment shader sampling | Yes |
| Per-cascade uniforms | Uniform arrays | Yes |

**No compute shaders needed.** The contact shadow ray march runs in the fragment shader during the lighting pass. This is how most implementations work (including the Spartan engine implementation by Panos Karabelas).

---

## Common Pitfalls

### Pitfall 1: Fixed World-Space Bias Values
**What goes wrong:** Bias that works for cascade 0 is wrong for cascade 2 (10-20x larger world-space coverage).
**Why it happens:** Treating bias as a world-space constant instead of a texel-space proportion.
**How to avoid:** Always scale bias/offset by the cascade's texel size: `cascadeRange / shadowMapResolution`.
**Warning signs:** Artifacts that only appear in far cascades, or Peter Panning only in near cascade.

### Pitfall 2: Normal Offset on Wrong Normal
**What goes wrong:** Using normal-mapped normals instead of geometric (interpolated vertex) normals for shadow offset.
**Why it happens:** The fragment shader often has both available and it is easy to use the wrong one.
**How to avoid:** Always use the geometric surface normal (from vertex shader, before normal mapping) for shadow bias calculations.
**Warning signs:** Shadow acne that follows normal map detail patterns.

### Pitfall 3: Caster-Side Offset Creates Coupled Regressions
**What goes wrong:** Fixing one junction type breaks another; values are interdependent.
**Why it happens:** Moving geometry in the shadow map is a global perturbation that affects all surfaces.
**How to avoid:** Do not modify shadow caster geometry. Use receiver-side techniques only.
**Warning signs:** "Whack-a-mole" debugging where fixing wall-ceiling breaks wall-floor.

### Pitfall 4: Inadequate Depth Precision from Loose Near/Far Planes
**What goes wrong:** Shadow gaps appear at junctions even with correct bias because depth quantization is too coarse.
**Why it happens:** The cascade's orthographic projection has near/far planes much wider than necessary (current engine uses zMult=10.0).
**How to avoid:** Use GL_DEPTH_CLAMP and reduce zMult. Tighter depth range = more depth precision = smaller quantization gaps.
**Warning signs:** Artifacts that improve when shadow map resolution is increased (confirms precision is the bottleneck).

### Pitfall 5: Contact Shadows Without Thickness Threshold
**What goes wrong:** Contact shadows darken the entire backside of objects (false occlusion).
**Why it happens:** The ray march detects occlusion wherever the ray goes behind any geometry in the depth buffer, even if the occluder is far behind.
**How to avoid:** Include a thickness threshold: only count as occluded if the depth difference is small (0.01-0.05 world units).
**Warning signs:** Dark halos on the shadow-side of large objects.

---

## Project Constraints (from CLAUDE.md)

- **Graphics API:** OpenGL 4.1 Core Profile (macOS ceiling). All techniques must use GLSL 4.10.
- **Shader location:** Engine shaders in `assets/shaders/engine/`, game shaders in `assets/shaders/game/`.
- **No external test framework:** Tests are standalone executables.
- **RAII for GL resources:** Any new FBOs or textures must be cleaned up in destructors.
- **Naming conventions:** PascalCase for classes/files, camelCase for methods, snake_case for variables.
- **No commits as co-author:** Per user CLAUDE.md directive.

---

## Sources

### Primary (HIGH confidence)
- [Microsoft: Common Techniques to Improve Shadow Depth Maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps) -- Comprehensive reference on bias, near/far planes, texel snapping, geometry recommendations
- [MJP: A Sampling of Shadow Techniques](https://therealmjp.github.io/posts/shadow-maps/) -- Normal offset shadows, receiver plane depth bias, VSM/EVSM comparison, shadow pancaking
- [Render Diagrams: Shadowmap Bias (Dec 2024)](https://renderdiagrams.org/2024/12/18/shadowmap-bias/) -- Receiver plane depth bias with per-texel correction, bilinear PCF, mathematical derivation
- [DigitalRune: Shadow Acne](https://digitalrune.github.io/DigitalRune-Documentation/html/3f4d959e-9c98-4a97-8d85-7a73c26145d7.htm) -- Normal offset technique, depth bias + slope-scaled offset combination
- [LearnOpenGL: Shadow Mapping](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping) -- PCF, bias techniques, front/back face culling for shadow maps
- [LearnOpenGL: Cascaded Shadow Maps](https://learnopengl.com/Guest-Articles/2021/CSM) -- Cascade splitting, stable cascades, cascade blending

### Secondary (MEDIUM confidence)
- [WillP GFX: Dealing with Shadow Map Artifacts](https://willpgfx.com/2015/05/dealing-with-shadow-map-artifacts/) -- Normal offset implementation with per-light scaling
- [ndotl: Notes on Shadow Bias](https://ndotl.wordpress.com/2014/12/19/notes-on-shadow-bias/) -- Receiver plane depth bias vs normal offset comparison, edge failure cases
- [Panos Karabelas: Screen Space Shadows](https://panoskarabelas.com/posts/screen_space_shadows/) -- Contact shadow ray marching implementation, performance characteristics
- [Alex Tardif: Cascaded Shadow Maps with Soft Shadows](https://alextardif.com/shadowmapping.html) -- Practical CSM implementation details
- [NVIDIA: Cascaded Shadow Maps (Rouslan Dimitrov)](https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf) -- Reference NVIDIA implementation

### Tertiary (LOW confidence -- unverified claims)
- The Witness blog shadow mapping posts (connection refused during research, referenced by multiple secondary sources)
- Godot engine shadow bias PR #51025 (referenced approach but not verified against current source)
- UE5 Virtual Shadow Maps internals (documented behavior, source not inspected)

---

## Metadata

**Confidence breakdown:**
- Root cause analysis: HIGH -- well-documented geometric problem with clear mathematical explanation
- Professional engine approaches: MEDIUM-HIGH -- based on documentation and community sources, not direct source code inspection
- Recommended techniques: HIGH -- all techniques verified against multiple authoritative sources and OpenGL 4.1 compatibility confirmed
- Implementation specifics: MEDIUM -- code examples are illustrative, will need adaptation to engine's specific rendering pipeline

**Research date:** 2026-04-06
**Valid until:** 2026-10-06 (shadow mapping is a mature field; techniques are stable)
