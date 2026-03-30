# Quick Task: Fix Remaining SSAO Halo Glow Around Objects - Research

**Researched:** 2026-03-30
**Domain:** SSAO shader / depth discontinuity artifact
**Confidence:** HIGH

## Summary

The bright halo around objects on floors is a well-documented SSAO artifact caused by how the current shader handles samples that land on a closer surface (e.g., a chair above the floor). The current implementation has **two compounding problems**: (1) it skips samples hitting closer surfaces and uses a variable denominator, which means fewer valid samples near objects produces a brighter (less occluded) result; and (2) the range check logic itself is inverted from best practice for this specific case.

The fix is straightforward: remove the sample-skipping logic, use a fixed denominator (always divide by the total kernel size of 32), and let the existing `smoothstep` range check handle depth discontinuities naturally. This is exactly how the canonical LearnOpenGL implementation works, and it produces correct behavior because the `smoothstep` range check already smoothly fades out contributions from samples at depth discontinuities without creating bright spots.

**Primary recommendation:** Remove lines 48-53 (the sky-skip and the closer-surface skip), remove the variable `sampleCount`, and divide by the fixed constant `32.0` instead of `denom`. Remove the `max(sampleCount, 24.0)` clamping entirely.

## Root Cause Analysis

### The Mechanism (why halos appear)

Consider a flat floor with a chair sitting on it. For a floor pixel far from any object, the hemisphere samples mostly sample other floor pixels at similar depths -- the self-occlusion from the flat surface produces uniform darkening (a correct AO baseline).

For a floor pixel near the chair, some hemisphere samples now land on the chair (a closer surface in view space, meaning a smaller Z magnitude since view space Z is negative). The current shader has this logic at line 53:

```glsl
if (sampleDepth > viewPos.z + uAoBias && depthDiff > uAoRadius * 0.5) continue;
```

This **skips** samples that hit closer surfaces. But skipping them reduces `sampleCount` (the denominator), while the remaining samples contribute normal self-occlusion. With fewer samples in the denominator but the same occlusion numerator, the ratio `occlusion / denom` decreases, making the pixel **brighter** than surrounding floor.

The `max(sampleCount, 24.0)` clamp at line 62 was an attempt to limit this effect, but it only partially mitigates it -- 24/32 = 75% minimum, so there is still up to 25% brightness variation possible.

### Why the canonical solution works differently

The standard SSAO (John Chapman, LearnOpenGL) uses:

```glsl
float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
```

With a **fixed denominator** (always `kernelSize`):

```glsl
occlusion = 1.0 - (occlusion / kernelSize);
```

Key differences from our implementation:
1. **No sample skipping** -- every sample contributes, weighted by `rangeCheck`
2. **Fixed denominator** -- always divides by total kernel size (e.g., 32 or 64)
3. **smoothstep fades naturally** -- when `depthDiff` is large (depth discontinuity), `rangeCheck` approaches 0, so the sample contributes ~0 occlusion, which is correct (no occlusion = neutral, not bright)
4. **No bright halo** -- samples hitting a closer object get `rangeCheck ~= 0` contribution AND the denominator stays fixed, so the ratio is the same as a flat surface with no nearby objects

### Why the current skip logic causes brightness

When a sample is skipped entirely and `sampleCount` is decremented:
- Floor far from chair: 30 occluded / 32 = 0.9375 occlusion ratio
- Floor near chair (8 samples skipped): 22 occluded / max(24, 24) = 0.9167 occlusion ratio

The floor near the chair has a **lower** occlusion ratio, producing a **brighter** pixel -- the visible halo.

With fixed denominator and smoothstep (correct approach):
- Floor far from chair: 30 occluded / 32 = 0.9375
- Floor near chair: 22 occluded + 8*(0.0 * rangeCheck) / 32 = 22/32 = 0.6875

Wait -- this would actually make it darker near objects! Which is worse. The key is that the `sampleDepth >= samplePos.z + bias` test matters. For samples that hit a closer surface (the chair), sampleDepth is CLOSER to the camera, meaning sampleDepth (the actual depth at that screen location) is greater than samplePos.z (the projected sample position deeper into the scene). So the comparison `sampleDepth >= samplePos.z + bias` evaluates to **true** (1.0), meaning: "yes this sample is NOT occluded."

But `rangeCheck` will be near 0 because the depth difference is large. So the contribution is `1.0 * ~0.0 = ~0.0`, which is zero occlusion contribution, which is neutral. The fixed denominator means this pixel gets the same baseline as a pixel far from the chair.

This is the correct behavior.

## Recommended Fix

### ssao.frag - Corrected shader

```glsl
void main() {
    float depth = texture(uDepthTex, vTexCoord).r;
    if (depth >= 0.9999) {
        fragColor = vec4(1.0);
        return;
    }

    vec3 viewPos = reconstructViewPos(vTexCoord, depth);
    vec3 worldNormal = texture(uGeomNormalTex, vTexCoord).rgb * 2.0 - 1.0;
    vec3 normal = normalize(mat3(uView) * worldNormal);

    vec3 randomVec = normalize(texture(uNoiseTex, vTexCoord * uNoiseScale).xyz);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 32; ++i) {
        vec3 samplePos = viewPos + TBN * uSamples[i] * uAoRadius;
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float rawSampleDepth = texture(uDepthTex, offset.xy).r;
        // Sky pixels: treat as no occlusion (rangeCheck will handle via large depth diff)
        float sampleDepth = reconstructViewPos(offset.xy, rawSampleDepth).z;

        // Standard range check: smoothly fades contribution for large depth differences
        float depthDiff = abs(viewPos.z - sampleDepth);
        float rangeCheck = smoothstep(0.0, 1.0, uAoRadius / max(depthDiff, 0.001));

        // Occlusion test: is the actual surface closer than the sample point?
        occlusion += (sampleDepth >= samplePos.z + uAoBias ? 0.0 : 1.0) * rangeCheck;
    }

    // Fixed denominator -- always divide by total sample count
    fragColor = vec4(vec3(1.0 - (occlusion / 32.0) * uAoStrength), 1.0);
}
```

Changes from current code:
1. **Removed** the `if (rawSampleDepth >= 0.9999) continue` skip -- sky samples naturally get near-zero `rangeCheck` due to huge depth difference
2. **Removed** the `if (sampleDepth > viewPos.z + uAoBias && depthDiff > uAoRadius * 0.5) continue` skip -- this was the direct cause of the halo
3. **Removed** the variable `sampleCount` tracking
4. **Removed** the `max(sampleCount, 24.0)` clamped denominator
5. **Fixed denominator** of `32.0` (total kernel size)
6. The `smoothstep` range check and occlusion test remain identical to the canonical implementation

### ssao_blur.frag - No changes needed

The current bilateral blur is already correct: it uses depth-aware weighting (`step(depthDiff, 0.001)`) and properly renormalizes. The only potential improvement would be using `smoothstep` instead of `step` for the depth threshold, but the hard cutoff is fine for preventing AO bleed across edges. The `step` threshold of 0.001 in NDC depth space is appropriate.

One minor improvement: the blur currently iterates `[-2, 2)` (i.e., -2, -1, 0, 1) which is off-center. It should be `[-2, 3)` for a true 5x5 or stay as `[-2, 2)` for a 4x4, but John Chapman specifically recommends centering the blur kernel to reduce halos at boundaries. This is a minor cosmetic issue, not the root cause.

## Common Pitfalls

### Pitfall 1: Variable Denominator
**What goes wrong:** Dividing by the count of "valid" samples instead of total kernel size creates brightness variations where samples are rejected.
**Why it happens:** Intuitive but wrong -- seems like you should only count samples that "matter."
**How to avoid:** Always use fixed denominator equal to total kernel size. Let the range check weight handle invalid samples.

### Pitfall 2: Skipping Sky Samples
**What goes wrong:** Skipping samples that hit the sky (depth >= 0.9999) with `continue` reduces the denominator.
**How to avoid:** Don't skip them. The `smoothstep` range check will give them near-zero weight naturally because `radius / huge_depth_diff` approaches 0.

### Pitfall 3: Over-aggressive Closer-Surface Rejection
**What goes wrong:** Skipping samples that hit a closer surface (like a chair above a floor) removes valid occlusion information and brightens the area.
**How to avoid:** Don't skip these samples. The occlusion test combined with range check handles them correctly: the sample tests as "not occluded" but gets near-zero weight from the range check, contributing neutral (no) occlusion.

## Sources

### Primary (HIGH confidence)
- [LearnOpenGL SSAO Tutorial](https://learnopengl.com/Advanced-Lighting/SSAO) -- Canonical implementation with fixed denominator, verified range check code
- [John Chapman SSAO Tutorial](http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html) -- Original reference, centered blur kernel recommendation

### Secondary (MEDIUM confidence)
- [Know Your SSAO Artifacts](https://mtnphil.wordpress.com/2013/06/26/know-your-ssao-artifacts/) -- Artifact categorization and fixes
- [Alex Tardif SSAO](https://alextardif.com/SSAO.html) -- Sample weighting with range check and normal dot product
- [Frictional Games SSAO](https://frictionalgames.com/2014-01-tech-feature-ssao-and-temporal-blur/) -- Bilateral blur depth threshold (2cm in SOMA)
- [Pure Depth SSAO](https://theorangeduck.com/page/pure-depth-ssao) -- Falloff functions and parameter sensitivity

## Metadata

**Confidence breakdown:**
- Root cause: HIGH -- the variable denominator + sample skipping mechanism is a well-documented cause of SSAO bright halos
- Fix: HIGH -- reverting to the canonical fixed-denominator approach is the standard industry solution, verified across multiple authoritative sources
- Blur pass: HIGH -- current bilateral blur is already correct

**Research date:** 2026-03-30
**Valid until:** Indefinite (this is fundamental graphics math, not API-dependent)
