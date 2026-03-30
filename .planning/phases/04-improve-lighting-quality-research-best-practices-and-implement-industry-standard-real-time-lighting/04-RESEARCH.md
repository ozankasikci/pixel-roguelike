# Phase 4: Improve Lighting Quality - Research

**Researched:** 2026-03-30
**Domain:** Real-time rendering — shadows (PCF/PCSS/CSM), ambient occlusion (SSAO), bloom (mip-chain), area/tube lights (LTC/closest-point)
**Confidence:** HIGH overall — all major techniques verified against official documentation and reference implementations

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Shadows should be soft and diffuse — PCF with large kernel or PCSS.
- **D-02:** Add cascaded shadow maps (CSM) for the sun/directional light.
- **D-03:** Expand beyond current 2 spot shadow limit — spot + directional shadow support. Point light shadows are NOT in scope.
- **D-04:** Subtle contact AO — gentle darkening. Not heavy-handed.
- **D-05:** SSAO technique is Claude's discretion (classic SSAO vs HBAO vs GTAO).
- **D-06:** Soft warm glow via multi-pass Gaussian downsample/upsample chain (Unreal-style bloom).
- **D-07:** Replace the current crude 8-sample bloom tap with a proper mip-chain bloom pipeline.
- **D-08:** Bloom is configurable per-environment.
- **D-09:** Add rectangular area lights.
- **D-10:** Research and implement tube/line lights if feasible within OpenGL 4.1.
- **D-11:** Research emissive mesh lighting — implement if achievable with good visual return.
- **D-12:** Claude has discretion on which new light types to actually implement.
- **D-13:** Keep arbitrary intensity values (current float system).

### Claude's Discretion
- SSAO technique selection (classic SSAO vs HBAO vs GTAO)
- Shadow map resolution and cascade count for CSM
- Bloom mip chain depth and threshold tuning
- Which of the new light types (area, tube, emissive) to actually implement
- Soft shadow technique (PCF kernel size vs PCSS)
- Any performance optimizations needed to maintain frame rate with new features
- AO sample count, radius, and blur approach

### Deferred Ideas (OUT OF SCOPE)
- Point light shadow maps (cubemap shadows)
- Physically-based light units (lumens/lux)
- Light probes / IBL (image-based lighting)
- Screen-space reflections
- Volumetric lighting / god rays
</user_constraints>

---

## Summary

This phase upgrades four independent subsystems in the existing OpenGL 4.1 forward renderer. The engine already provides a solid foundation: Cook-Torrance PBR, an FBO with color+depth+normal textures, a two-pass post-process chain (composite → stylize), and a per-environment parameter system.

**Shadows** need two changes: (a) soft PCF with a larger Poisson-disk kernel replacing the current 3x3 box, and (b) a new CSM system for the directional sun using `GL_TEXTURE_2D_ARRAY` and a geometry shader with `invocations`. The existing per-spot shadow maps stay unchanged; directional CSM is additive.

**SSAO** fits naturally between the scene pass and composite pass. The engine's existing `normalTex_` on `sceneFBO_` provides world-space normals already — this is the biggest advantage over a typical forward renderer SSAO setup. Classic hemisphere SSAO (not HBAO or GTAO) is the right choice: it runs comfortably in OpenGL 4.1 without compute shaders, provides the subtle contact-darkening the art direction requires, and is 4–7x cheaper than GTAO.

**Bloom** replaces `bloomGlow()` in composite.frag with a multi-pass mip-chain approach (5 mip levels, 13-tap downsample, 3x3 tent upsample). This is a pure shader + intermediate FBO change with no C++ API changes except adding bloom FBO management to `CompositePass`.

**Area and tube lights** should use the LTC (Linearly Transformed Cosines) technique for rectangular area lights and the closest-point-on-segment approximation for tube lights. Both extend the existing `RenderLight` struct and `LightType` enum. Emissive mesh "lighting" is not a real light type — it should be implemented as a material property that adds to `totalLight` directly in scene.frag, using bloom to create the perceived glow effect.

**Primary recommendation:** Implement in this order: (1) PCF soft shadows + increase spot shadow count, (2) SSAO pass, (3) mip-chain bloom, (4) CSM for directional light, (5) area/tube light types. Each is independently shippable.

---

## Project Constraints (from CLAUDE.md)

| Constraint | Implication for this phase |
|------------|---------------------------|
| OpenGL 4.1 Core Profile only | No compute shaders — SSAO must use fragment shader. Geometry shader instancing for CSM is available in 4.1. `GL_TEXTURE_2D_ARRAY` is available. |
| GLSL `#version 410 core` | All new shaders use `#version 410 core`. No `gl_FragDepth` overwrite restrictions apply — CSM depth pass is standard. `sampler2DArray` is available in GLSL 4.10. |
| macOS OpenGL ceiling | Confirmed: geometry shaders with `layout(triangles, invocations = N)` are available on macOS OpenGL 4.1. |
| Custom C++ engine | No Unity/Unreal helpers — all techniques must be implemented from scratch in GLSL + C++. |
| Cook-Torrance PBR in scene.frag | New light types (area, tube) must integrate into the existing `for (int i = 0; i < uNumLights; ++i)` loop and use the same BRDF functions already defined. |
| NamingConventions | Classes: PascalCase. Functions: camelCase. Private members: trailing underscore. Shaders: `#version 410 core`. RAII for all OpenGL resources. |
| No external test framework | Tests are standalone executables with exit-code pass/fail. No new test framework needed. |
| ACES tonemapping already applied | Bloom must be added BEFORE tonemapping in the composite pass (HDR bloom, not LDR bloom). |

---

## Standard Stack

All techniques are implemented in GLSL + C++, using the existing engine libraries. No new library dependencies are required.

### Core (no changes needed)
| Library | Version | Purpose | Status |
|---------|---------|---------|--------|
| OpenGL 4.1 Core | 4.1 | Graphics API | Already in use |
| GLFW 3.4 | 3.4 | Window/context | Already in use |
| GLM 1.0.3 | 1.0.3 | Math | Already in use — use `glm::ortho`, `glm::frustum`, `glm::inverse` |
| GLAD 2 | v2.0.8 | Function loader | Already in use |

### LTC Lookup Tables (static data, not a library)
The LTC area light technique requires two precomputed 64x64 RGBA float textures:
- `LTC1`: encodes the inverse transformation matrix (4 floats → packed as vec4)
- `LTC2`: encodes Fresnel, horizon-clip factor, and Smith GGX coefficient

These are static C++ arrays baked from the reference implementation. They are embedded in source or loaded from asset files — no runtime computation. Source: [selfshadow/ltc_code](https://github.com/selfshadow/ltc_code) (MIT-compatible).

---

## Architecture Patterns

### Recommended Integration into Existing Pipeline

```
Per-frame render order:
1. Shadow pass (spot lights + CSM directional)    [existing: extend]
2. Scene pass → sceneFBO_ (color + depth + normals)  [existing: unchanged]
3. SSAO pass → ssaoRawFBO_ (new)
4. SSAO blur pass → ssaoFBO_ (new)
5. Composite pass (bloom mip-chain, tonemap, fog...)  [existing: extend]
6. Stylize pass → screen                             [existing: unchanged]
```

### Recommended Project Structure (additions only)
```
assets/shaders/engine/
├── composite.frag         (modified: replace bloomGlow, add bloom mip chain)
├── ssao.frag              (new: SSAO hemisphere sampling)
├── ssao_blur.frag         (new: 4x4 box blur pass)
├── bloom_downsample.frag  (new: 13-tap downsample kernel)
├── bloom_upsample.frag    (new: 3x3 tent upsample kernel)
├── csm_depth.vert         (new: CSM shadow depth vertex)
├── csm_depth.geom         (new: geometry shader, invocations = N cascades)
├── csm_depth.frag         (new: trivial depth-only frag)
└── scene.frag             (modified: soft shadows, new light types, SSAO sampling)

src/engine/rendering/
├── lighting/
│   ├── ShadowMap.h/.cpp   (existing)
│   └── CascadedShadowMap.h/.cpp  (new: manages GL_TEXTURE_2D_ARRAY + cascade matrices)
├── post/
│   ├── CompositePass.h/.cpp  (modified: bloom FBOs, SSAO texture input)
│   ├── SsaoPass.h/.cpp        (new: SSAO + blur FBOs and shaders)
│   └── BloomPass.h/.cpp       (new: mip-chain FBOs and shaders)

src/engine/rendering/lighting/
└── RenderLight.h   (modified: add AreaRect, Tube to LightType enum; add area light fields)

src/game/components/
└── LightComponent.h  (modified: add area/tube fields)
```

### Pattern 1: Soft Shadows — PCF with Poisson Disk

**What:** Replace the existing 3x3 box-filter PCF with a 16-sample rotated Poisson disk. Optionally add PCSS blocker search for distance-dependent penumbra width.

**When to use:** Always — replaces the existing `sampleShadow()` function in scene.frag.

**Recommendation:** Use Poisson-disk PCF (not PCSS) as the default. PCSS adds a blocker search pass (inner loop) and produces banding at medium sample counts. For the Stanley Parable aesthetic — soft, uniform shadow edges — a fixed large-kernel Poisson PCF produces better-looking results with no per-frame variance.

```glsl
// Source: NVIDIA GPU Gems 2 Ch.17 / fabiensanglard.net/shadowmappingPCF
// 16-tap rotated Poisson disk PCF
const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

float sampleShadowPCF(int shadowIndex, vec3 projCoords, float bias) {
    float visibility = 0.0;
    vec2 texel = shadowTexelSize(shadowIndex);
    float spread = 3.0; // kernel spread in texels
    // Random rotation per fragment to break banding
    float angle = hash13(vec3(gl_FragCoord.xy, 0.0)) * 6.2831;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, s, -s, c);
    for (int i = 0; i < 16; ++i) {
        vec2 offset = (rot * poissonDisk[i]) * texel * spread;
        float storedDepth = shadowDepthAt(shadowIndex, projCoords.xy + offset);
        visibility += (projCoords.z - bias) <= storedDepth ? 1.0 : 0.0;
    }
    return visibility / 16.0;
}
```

### Pattern 2: Cascaded Shadow Maps (CSM) for Directional Light

**What:** Split the view frustum into N depth ranges (cascades), render a separate shadow map for each cascade into one `GL_TEXTURE_2D_ARRAY` layer, select cascade in scene.frag by fragment view-space depth.

**Recommendation:** 3 cascades. Cascade distances: near/10%, near/40%, far. This covers close contact shadows, mid-range, and distant windows. More than 3 cascades is overkill for this scene scale.

**C++ shadow map storage:**
```cpp
// Source: learnopengl.com/Guest-Articles/2021/CSM
// Create a 2D array texture: resolution x resolution x numCascades
glGenTextures(1, &csmDepthArray_);
glBindTexture(GL_TEXTURE_2D_ARRAY, csmDepthArray_);
glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
             resolution, resolution, kCsmCascades, 0,
             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

// Attach to FBO (layered attachment — all layers at once)
glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, csmDepthArray_, 0);
```

**Geometry shader for multi-layer rendering (one draw call):**
```glsl
// Source: learnopengl.com/Guest-Articles/2021/CSM
#version 410 core
layout(triangles, invocations = 3) in;   // 3 = kCsmCascades
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 uLightSpaceMatrices[3];

void main() {
    gl_Layer = gl_InvocationID;
    for (int i = 0; i < 3; ++i) {
        gl_Position = uLightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
```

**Cascade selection in scene.frag:**
```glsl
// Source: learnopengl.com/Guest-Articles/2021/CSM
uniform sampler2DArrayShadow uCsmShadowMap;
uniform mat4 uCsmMatrices[3];
uniform float uCsmSplitDistances[3]; // view-space depths
uniform int uCsmCascadeCount;

int selectCascade(vec3 fragViewPos) {
    float depth = abs(fragViewPos.z);
    for (int i = 0; i < uCsmCascadeCount; ++i) {
        if (depth < uCsmSplitDistances[i]) return i;
    }
    return uCsmCascadeCount - 1;
}

float sampleCsmShadow(vec3 fragViewPos, vec3 N, vec3 L) {
    int layer = selectCascade(fragViewPos);
    vec4 fragInLightSpace = uCsmMatrices[layer] * vec4(vWorldPos, 1.0);
    vec3 projCoords = fragInLightSpace.xyz / fragInLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z >= 1.0) return 1.0;
    // Bias scales with cascade — farther cascades need less bias
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005) / uCsmSplitDistances[layer];
    return texture(uCsmShadowMap, vec4(projCoords.xy, float(layer), projCoords.z - bias));
}
```

**Critical pitfall — bias per cascade:** Each cascade covers a different depth range. A fixed bias that works for cascade 0 causes Peter-Panning in cascade 2. Scale bias inversely proportional to each cascade's far plane distance.

### Pattern 3: SSAO Pass

**What:** Render a hemisphere of random samples around each fragment's surface normal, test sample depths against the depth buffer, accumulate occlusion factor. Follow with a blur pass to remove noise.

**Technique recommendation: Classic hemisphere SSAO (Crytek/LearnOpenGL variant)**

Reasoning:
- HBAO+ requires 2D horizon marching — significantly more complex, needs more samples, ~3x the cost of classic SSAO.
- GTAO is 4–7x more expensive than classic SSAO on equivalent hardware. On macOS OpenGL with iGPU or mid-range dGPU, GTAO at 1080p costs 2–7ms. Classic SSAO costs 1–2ms. For the "barely noticeable" AO goal, classic SSAO is the right fit.
- Classic SSAO with 16–32 samples + 4x4 blur produces the subtle contact-darkening D-04 requires.

**Key advantage specific to this engine:** The scene FBO already renders a normal buffer (`layout(location = 1) out vec4 fragNormal` in scene.frag, stored in `sceneFBO_.normalTexture()`). This is the `fragNormal` texture that already passes through to `CompositePass::apply()` and `StylizePass::apply()`. SSAO can read normals directly — no extra G-buffer pass needed.

**The normals in fragNormal.rgb are world-space** (encoded as `N * 0.5 + 0.5`). For SSAO, view-space normals work better for consistent sampling. Either convert in the SSAO shader (`mat3(uViewMatrix) * (N * 2.0 - 1.0)`) or add a view-space normal variant.

**Position reconstruction from depth (no position G-buffer needed):**
```glsl
// Source: theorangeduck.com/page/pure-depth-ssao, ogldev.org tutorial 46
vec3 reconstructViewPos(vec2 texCoord, float depth, mat4 invProj) {
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProj * ndc;
    return viewPos.xyz / viewPos.w;
}
```

**Core SSAO shader:**
```glsl
// Source: learnopengl.com/Advanced-Lighting/SSAO
uniform sampler2D uDepthTex;
uniform sampler2D uNormalTex;   // sceneFBO_.normalTexture() (world-space)
uniform sampler2D uNoiseTex;    // 4x4 random rotation vectors, GL_REPEAT
uniform vec3 uSamples[32];      // hemisphere kernel in tangent space
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uInvProjection;
uniform vec2 uNoiseScale;       // screen_size / 4.0

void main() {
    vec2 uv = vTexCoord;
    float depth = texture(uDepthTex, uv).r;
    if (depth >= 0.9999) { fragColor = vec4(1.0); return; } // sky

    vec3 viewPos = reconstructViewPos(uv, depth, uInvProjection);
    vec3 worldNormal = texture(uNormalTex, uv).rgb * 2.0 - 1.0;
    vec3 normal = normalize(mat3(uView) * worldNormal); // world → view space

    vec3 randomVec = normalize(texture(uNoiseTex, uv * uNoiseScale).xyz);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 32; ++i) {
        vec3 samplePos = viewPos + TBN * uSamples[i] * uAoRadius;
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        float sampleDepth = reconstructViewPos(offset.xy, texture(uDepthTex, offset.xy).r, uInvProjection).z;
        float rangeCheck = smoothstep(0.0, 1.0, uAoRadius / abs(viewPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uAoBias) ? 0.0 : 1.0 * rangeCheck;
    }
    fragColor = vec4(vec3(1.0 - (occlusion / 32.0) * uAoStrength), 1.0);
}
```

**SSAO integration:** The AO texture multiplies the ambient term in scene.frag OR is applied as a multiplier in composite.frag. Applying it in composite.frag is simpler (no scene shader changes for SSAO connection). Apply as: `color *= texture(uSsaoTex, vTexCoord).r;` before bloom.

**Blur pass:** 4x4 box filter in a second shader pass. Sample the 4x4 region and average — removes the 4x4 tile noise pattern from the rotation texture.

### Pattern 4: Mip-Chain Bloom

**What:** Replace `bloomGlow()` in composite.frag with a multi-pass pipeline: (1) extract bright pixels, (2) progressively downsample 5 times using 13-tap kernel, (3) upsample back using 3x3 tent filter with additive blending, (4) composite.

**Source:** [LearnOpenGL Physically Based Bloom](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)

**Architecture change:** The current bloom is a single-pass inline function in composite.frag. The new bloom needs multiple intermediate FBOs (one per mip level). This means `BloomPass` becomes a separate C++ class with its own set of downscale FBOs, similar to how `CompositePass` is structured.

**Downsample kernel (13 taps, eliminate temporal pulsing):**
```glsl
// Source: learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom (COD: Advanced Warfare technique)
vec3 downsample(sampler2D src, vec2 uv, vec2 texelSize) {
    // a-b-c / d-e-f / g-h-i with 4 intermediate bilinear samples j,k,l,m
    vec3 a = texture(src, uv + vec2(-2.0, -2.0) * texelSize).rgb;
    vec3 b = texture(src, uv + vec2( 0.0, -2.0) * texelSize).rgb;
    vec3 c = texture(src, uv + vec2( 2.0, -2.0) * texelSize).rgb;
    vec3 d = texture(src, uv + vec2(-2.0,  0.0) * texelSize).rgb;
    vec3 e = texture(src, uv + vec2( 0.0,  0.0) * texelSize).rgb;
    vec3 f = texture(src, uv + vec2( 2.0,  0.0) * texelSize).rgb;
    vec3 g = texture(src, uv + vec2(-2.0,  2.0) * texelSize).rgb;
    vec3 h = texture(src, uv + vec2( 0.0,  2.0) * texelSize).rgb;
    vec3 i = texture(src, uv + vec2( 2.0,  2.0) * texelSize).rgb;
    vec3 j = texture(src, uv + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 k = texture(src, uv + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 l = texture(src, uv + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 m = texture(src, uv + vec2( 1.0,  1.0) * texelSize).rgb;

    return e * 0.125 + (a + c + g + i) * 0.03125
         + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
}
```

**Upsample kernel (3x3 tent filter):**
```glsl
// Source: learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
vec3 upsample(sampler2D src, vec2 uv, vec2 texelSize, float filterRadius) {
    float x = filterRadius;
    float y = filterRadius;
    vec3 a = texture(src, uv + vec2(-x,  y)).rgb;
    vec3 b = texture(src, uv + vec2( 0,  y)).rgb * 2.0;
    vec3 c = texture(src, uv + vec2( x,  y)).rgb;
    vec3 d = texture(src, uv + vec2(-x,  0)).rgb * 2.0;
    vec3 e = texture(src, uv + vec2( 0,  0)).rgb * 4.0;
    vec3 f = texture(src, uv + vec2( x,  0)).rgb * 2.0;
    vec3 g = texture(src, uv + vec2(-x, -y)).rgb;
    vec3 h = texture(src, uv + vec2( 0, -y)).rgb * 2.0;
    vec3 i = texture(src, uv + vec2( x, -y)).rgb;
    return (a + b + c + d + e + f + g + h + i) / 16.0;
}
```

**C++ bloom FBO chain:** 5 FBOs, each at half the previous resolution. The final composite shader reads the smallest mip level output and adds it to the scene color. No threshold: the downsample naturally emphasizes bright pixels (HDR values > 1.0 dominate). Remove the existing `uBloomThreshold`-based extraction.

**Important:** Bloom must be applied in HDR space (before ACES tonemapping). The current composite.frag applies bloom after `color += bloomGlow(...) * uBloomIntensity` then `color = acesFitted(color)` — this is correct. The new bloom pass should output a texture that gets added at the same point.

### Pattern 5: Rectangular Area Lights (LTC)

**What:** Linearly Transformed Cosines — approximate the GGX BRDF integral over a polygonal light by transforming the integration domain using a precomputed matrix. Requires two 64x64 RGBA float LUT textures.

**Source:** [Eric Heitz LTC paper](https://eheitzresearch.wordpress.com/415-2/) / [LearnOpenGL Area Lights](https://learnopengl.com/Guest-Articles/2022/Area-Lights) / [selfshadow/ltc_code](https://github.com/selfshadow/ltc_code)

**LUT integration:** Two static textures (`ltc1` and `ltc2`), each 64x64 RGBA32F. Load from the precomputed C++ array from `selfshadow/ltc_code`. These are tiny (64x64x4x4 = 64KB each) and can live as static arrays in a source file.

```glsl
// Source: learnopengl.com/Guest-Articles/2022/Area-Lights
// LUT lookup by roughness (x) and NdotV (y)
vec2 ltcUv = vec2(roughness, sqrt(1.0 - NdotV));
ltcUv = ltcUv * (63.0 / 64.0) + 0.5 / 64.0; // remap to texel centers

vec4 t1 = texture(uLtcMat, ltcUv);
vec4 t2 = texture(uLtcAmp, ltcUv);

// Reconstruct the inverse transformation matrix
mat3 Minv = mat3(
    vec3(t1.x, 0, t1.y),
    vec3(   0, 1,    0),
    vec3(t1.z, 0, t1.w)
);

// Evaluate the polygon integral in the transformed space
vec3 diffuse  = LTC_Evaluate(N, V, vWorldPos, mat3(1.0),  areaLightCorners, twoSided);
vec3 specular = LTC_Evaluate(N, V, vWorldPos, Minv,       areaLightCorners, twoSided) * t2.x + ...;
```

**LightType extension for area lights:**
```cpp
enum class LightType {
    Point = 0,
    Spot = 1,
    Directional = 2,
    AreaRect = 3,   // NEW: rectangular area light
    Tube = 4,       // NEW: tube/line light (closest-point method)
};
```

**RenderLight struct additions for AreaRect:**
```cpp
struct RenderLight {
    // ... existing fields ...
    // Area light specific (AreaRect + Tube):
    glm::vec3 right{1.0f, 0.0f, 0.0f};   // light's local X axis
    glm::vec3 up{0.0f, 1.0f, 0.0f};      // light's local Y axis
    float width = 1.0f;                    // half-width for AreaRect, segment half-length for Tube
    float height = 0.5f;                   // half-height for AreaRect, tube radius for Tube
    bool doubleSided = false;
};
```

### Pattern 6: Tube Lights (Closest-Point Method)

**What:** Find the closest point on the tube's line segment to the surface reflection vector, use that as a representative point light. Adjust the specular distribution by the tube's "angular size" (representative sphere radius).

**Source:** [Alex Tardif Area Lights](https://alextardif.com/arealights.html) / [Wicked Engine 2017](https://wickedengine.net/2017/09/area-lights/)

The original Frostbite SIGGRAPH 2014 paper covers diffuse equations for tube lights using analytic form factors. For our use case (institutional fluorescent tubes, subtle fill lighting), the representative-sphere approximation is sufficient and much simpler.

```glsl
// Source: alextardif.com/arealights.html (adapted from Frostbite)
vec3 tubeLighting(RenderLight light, vec3 N, vec3 V, vec3 P,
                  vec3 albedo, float roughness, float metalness, vec3 F0) {
    vec3 L0 = light.position - light.right * light.width - P;
    vec3 L1 = light.position + light.right * light.width - P;

    float distL0 = length(L0);
    float distL1 = length(L1);
    float NdotL0 = dot(N, L0 / distL0);
    float NdotL1 = dot(N, L1 / distL1);
    float NdotL = (2.0 * clamp(NdotL0 + NdotL1, 0.0, 2.0))
                / (distL0 * distL1 + dot(L0, L1) + 2.0);

    // Representative point: closest point on segment to reflection ray
    vec3 R = reflect(-V, N);
    float t = clamp(dot(R, L0) / (dot(L0, L0) - dot(L0, L1)) + 0.5, 0.0, 1.0);
    vec3 closestPoint = L0 + (L1 - L0) * t;
    // Add tube radius offset along perpendicular
    float tubeRadius = light.height;
    vec3 centerToRay = closestPoint - R * dot(R, closestPoint);
    vec3 closestPointOnSphere = closestPoint + normalize(centerToRay)
                              * min(tubeRadius, length(centerToRay));
    vec3 L = normalize(closestPointOnSphere);

    // alphaPrime — representative sphere widens the specular lobe
    float alphaPrime = clamp(roughness + tubeRadius / (2.0 * length(closestPoint)), 0.0, 1.0);

    // Evaluate Cook-Torrance with modified L and alphaPrime
    // ... (same BRDF code as existing lighting loop) ...

    float falloff = attenuation(length(closestPoint), light.radius);
    return (diffuse + specular) * light.color * light.intensity * falloff;
}
```

### Pattern 7: Emissive Mesh Lighting

**Finding:** True emissive mesh lighting (where a mesh illuminates nearby surfaces) requires either radiosity, light probes, or global illumination — none of which are in scope. However, the visual goal (glowing light panels that seem to illuminate their surroundings) can be achieved with two simpler techniques:

1. **Emissive material contribution:** Add an `emissiveStrength` uniform to the material. In scene.frag, after the lighting loop: `totalLight += albedo * uEmissiveStrength;`. Combined with bloom, this creates a visible glow on the surface itself that bleeds into neighboring surfaces via the bloom pass.

2. **Paired area/tube lights:** Place a LightType::AreaRect or LightType::Tube at the same position as the emissive mesh. The mesh provides the visual light source shape; the paired area light provides actual illumination of surroundings. This is the standard technique in commercial engines (Unreal, Unity both use this pattern).

**Recommendation:** Implement emissive strength as a material property (simple uniform + bloom creates the effect). The "emissive mesh lighting" feature request is most efficiently served by the combination of emissive material + paired area light. No separate `LightType::Emissive` is needed.

### Anti-Patterns to Avoid

- **Adding SSAO position buffer as a separate G-buffer texture:** The engine already has the depth buffer and can reconstruct view-space position from it. A separate position texture wastes memory bandwidth.
- **Geometry-normal SSAO (no geometry normals):** Using the material normal map normals (detail + bumps) for SSAO hemisphere orientation causes excessive micro-occlusion and over-darkening. Use geometry normals (the `vNormal` data) — the `fragNormal` buffer encodes the post-normal-map normal, which may be too detailed. Consider adding a geometry-only normal option or use depth derivatives.
- **LDR bloom (applying bloom after tonemapping):** scene.frag writes HDR values. composite.frag applies bloom before `acesFitted()`. Keep this order — applying bloom to LDR-clamped values loses the HDR glow effect.
- **Fixed CSM bias:** Per-cascade bias tuning is mandatory. A single fixed bias value will either cause Peter-Panning on close cascades or shadow acne on distant cascades.
- **Hardcoding CSM invocation count in geometry shader:** Use a `#define` or shader preprocessing to make cascade count a build-time constant, not a magic number in the GS `invocations` layout.
- **Using `GL_TEXTURE_2D_ARRAY` with `glFramebufferTexture2D`:** For layered rendering (CSM), use `glFramebufferTexture()` (without the `2D`), which binds all layers. `glFramebufferTexture2D` only binds one layer at a time and defeats the geometry shader approach.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| LTC matrix fitting for area lights | Custom BRDF fitting tool | Static LUT arrays from `selfshadow/ltc_code` | LTC matrices take days of offline computation to fit correctly; the reference tables are validated against the GGX BRDF |
| Poisson disk distribution | Random sample generation | Hardcoded 16-sample Poisson disk (known good) | Randomly generated disks have clustering artifacts; the published 16-tap disk is validated for minimum distance distribution |
| CSM frustum splitting math | Custom frustum intersection | GLM `glm::ortho` + manual frustum corner extraction | GLM handles the projection math correctly; frustum corners are 8 `glm::vec4` points from NDC cube inverse-projected |
| Bloom FBO mip chain | Manual texture resizing | One FBO per mip level, `glViewport` per pass | OpenGL 4.1 doesn't support rendering directly to individual mip levels without FBO attachment per level |
| SSAO hemisphere kernel | Random hemisphere generation | Reproducible set of 32 samples with accelerating distribution | Random seeds change per-run; a fixed validated kernel with `mix(0.1, 1.0, (i/N)^2)` scale gives stable results |

**Key insight:** The LTC lookup tables are the single most important "don't hand-roll" item. The published tables encode a carefully fitted approximation to GGX over all roughness/NdotV combinations. Rolling your own produces visible BRDF energy errors at grazing angles.

---

## Common Pitfalls

### Pitfall 1: SSAO Normal Buffer Mismatch
**What goes wrong:** SSAO produces excessive dark halos or incorrect occlusion at material boundaries.
**Why it happens:** The `fragNormal` buffer encodes the post-normal-map perturbed normal, not the geometric surface normal. Using high-frequency bumped normals for the SSAO hemisphere TBN matrix causes micro-occlusion — small bumps occlude themselves.
**How to avoid:** Add a separate output for geometric normals in scene.frag (`layout(location = 2) out vec4 fragGeomNormal`) that writes `normalize(vNormal)` without normal mapping. Use this for SSAO. Alternatively, reconstruct normals from depth using cross(dFdx(viewPos), dFdy(viewPos)) — this gives geometry normals regardless of material.
**Warning signs:** AO looks grainy and has brick/stone surface patterns baked into it.

### Pitfall 2: CSM Cascade Seams
**What goes wrong:** Visible hard edges in shadows where cascade boundaries meet.
**Why it happens:** Different shadow map resolutions and biases between adjacent cascades produce discontinuous shadow edges.
**How to avoid:** Blend between adjacent cascades in a small overlap zone (typically 10% of each cascade's depth range). Sample both cascades and lerp. Also: stabilize cascade boundaries by snapping the orthographic projection to shadow map texel increments — this prevents shimmering as the camera moves.
**Warning signs:** Visible sharp band in shadows at a specific distance from camera.

### Pitfall 3: Bloom at Wrong Pipeline Stage
**What goes wrong:** Bloom looks washed out or objects that should glow don't appear to glow.
**Why it happens:** Bloom applied to tone-mapped (LDR, 0-1 clamped) color loses HDR highlights. An emissive value of 2.0 looks the same as 1.0 after tonemapping, so bloom can't distinguish truly bright from barely-above-white.
**How to avoid:** Apply bloom before `acesFitted()` in composite.frag. The bloom pass operates on the HDR scene color buffer where emissive/bright areas have values > 1.0.
**Warning signs:** Bloom threshold parameter has no visible effect on highly emissive surfaces.

### Pitfall 4: LTC Area Light Over-Brightness
**What goes wrong:** Area lights are far brighter than equivalent point lights at the same intensity.
**Why it happens:** LTC integration accounts for the light's solid angle, which is larger than a point light. Integrating over a 1m x 0.5m panel produces more total energy than a point light at the same intensity value.
**How to avoid:** Normalize by area: `radiance = light.color * light.intensity / (light.width * light.height * 4.0)`. Or just accept that area light intensity values will be scaled differently from point lights — D-13 explicitly keeps arbitrary units, so this is a tuning matter, not a bug.
**Warning signs:** Scenes with area lights added look overexposed; turning down intensity to near-zero before getting the right look.

### Pitfall 5: SSAO Temporal Stability
**What goes wrong:** AO flickers or shimmers on moving objects or when camera moves.
**Why it happens:** The 4x4 noise texture rotation per fragment means adjacent frames have different random rotations. Without temporal accumulation, any camera movement causes visible frame-to-frame AO variation.
**How to avoid:** Use a fixed-seed deterministic noise texture (not time-varying), apply a slightly larger blur kernel (5x5 instead of 4x4), and keep AO strength subtle (D-04). TAA would eliminate this but is out of scope. At low AO strength, flicker is imperceptible.
**Warning signs:** AO shows high-frequency grain that moves with camera.

### Pitfall 6: Geometry Shader Performance on macOS
**What goes wrong:** CSM render pass is unexpectedly slow on macOS with Apple GPU.
**Why it happens:** macOS Metal (underneath OpenGL) emulates geometry shaders in software. Multi-invocation geometry shaders (`invocations = N`) can be 2-3x slower than expected on Apple Silicon compared to discrete GPUs.
**How to avoid:** As a fallback, render CSM cascades in N separate draw calls (one per cascade, binding the target layer via `glFramebufferTextureLayer`). This is slower per-draw-call but avoids GS overhead. Profile both approaches. For 3 cascades at 1024 resolution, the N-pass approach is typically adequate.
**Warning signs:** Frame time increase from CSM is much larger than expected compared to benchmarks on other platforms.

### Pitfall 7: GLSL Sampler Array Indexing with Non-Constant Index
**What goes wrong:** Shadow sampling code crashes or produces driver errors when using a variable `shadowIndex` to index into `uShadowMaps[shadowIndex]`.
**Why it happens:** GLSL 4.10 allows texture array indexing with dynamic (non-constant) indices, but some macOS OpenGL drivers have issues with dynamic sampler indexing when the array has more than 2-3 elements. The current code works because it uses explicit `if (shadowIndex == 0) ... return ... else return ...` branching.
**How to avoid:** Keep the explicit branching pattern for sampler array indexing that already exists in scene.frag. Do not refactor to `texture(uShadowMaps[shadowIndex], uv)` — the current pattern is intentional.
**Warning signs:** Blank shadow maps, all-zero shadow sampling, or GPU crash on shadow-enabled lights.

---

## Code Examples

### Existing Shadow Sampling (baseline to replace/extend)
```glsl
// Current: 3x3 box PCF — replace with 16-tap Poisson disk
float visibility = 0.0;
for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
        vec2 offset = vec2(float(x), float(y)) * texel;
        float storedDepth = shadowDepthAt(shadowIndex, proj.xy + offset);
        visibility += (proj.z - bias) <= storedDepth ? 1.0 : 0.0;
    }
}
return visibility / 9.0;
```

### SSAO Kernel Generation (C++)
```cpp
// Source: learnopengl.com/Advanced-Lighting/SSAO
// Generate in SsaoPass::init()
std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
std::default_random_engine generator;
std::vector<glm::vec3> ssaoKernel;
for (int i = 0; i < 32; ++i) {
    glm::vec3 sample(
        randomFloats(generator) * 2.0f - 1.0f,
        randomFloats(generator) * 2.0f - 1.0f,
        randomFloats(generator)
    );
    sample = glm::normalize(sample);
    sample *= randomFloats(generator);
    float scale = float(i) / 32.0f;
    scale = glm::mix(0.1f, 1.0f, scale * scale);  // accelerating distribution
    ssaoKernel.push_back(sample * scale);
}
```

### SSAO Noise Texture (C++)
```cpp
// 4x4 random rotation vectors, tiled over screen
std::vector<glm::vec3> ssaoNoise;
for (int i = 0; i < 16; ++i) {
    glm::vec3 noise(
        randomFloats(generator) * 2.0f - 1.0f,
        randomFloats(generator) * 2.0f - 1.0f,
        0.0f  // rotation around z-axis
    );
    ssaoNoise.push_back(noise);
}
GLuint noiseTex;
glGenTextures(1, &noiseTex);
glBindTexture(GL_TEXTURE_2D, noiseTex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, ssaoNoise.data());
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

### CSM Frustum Corners Extraction (C++)
```cpp
// Source: learnopengl.com/Guest-Articles/2021/CSM
// Get the 8 frustum corners in world space for a given projection * view matrix
std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj,
                                                    const glm::mat4& view) {
    const glm::mat4 inv = glm::inverse(proj * view);
    std::vector<glm::vec4> corners;
    for (int x = 0; x < 2; ++x)
    for (int y = 0; y < 2; ++y)
    for (int z = 0; z < 2; ++z) {
        glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f,
                                        2.0f * y - 1.0f,
                                        2.0f * z - 1.0f, 1.0f);
        corners.push_back(pt / pt.w);
    }
    return corners;
}

// Then build cascade light-space matrix from frustum slice
glm::mat4 buildCascadeMatrix(const glm::vec3& lightDir,
                              const std::vector<glm::vec4>& corners) {
    glm::vec3 center(0.0f);
    for (const auto& c : corners) center += glm::vec3(c);
    center /= float(corners.size());

    glm::vec3 up = std::abs(lightDir.y) > 0.97f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
    glm::mat4 lightView = glm::lookAt(center - lightDir, center, up);

    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;
    for (const auto& c : corners) {
        glm::vec4 lc = lightView * c;
        minX = std::min(minX, lc.x); maxX = std::max(maxX, lc.x);
        minY = std::min(minY, lc.y); maxY = std::max(maxY, lc.y);
        minZ = std::min(minZ, lc.z); maxZ = std::max(maxZ, lc.z);
    }
    // Pull near plane back to capture shadow casters outside frustum
    constexpr float zMult = 10.0f;
    if (minZ < 0) minZ *= zMult; else minZ /= zMult;
    if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

    glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hard shadow / 1-tap PCF | 16-tap Poisson disk PCF | 2015–present in AAA | Soft shadow edges without PCSS cost |
| Separate shadow textures per cascade | GL_TEXTURE_2D_ARRAY + GS layered rendering | ~2012, standard since | Single draw call for all cascades |
| Point-light approximation for area lights | LTC (Linearly Transformed Cosines) | SIGGRAPH 2016 | Correct GGX shading for polygonal lights |
| Threshold-based bloom | Mip-chain downsample/upsample bloom | ~2014 (COD: AW) | Eliminates pulsing artifacts, more natural falloff |
| Sphere-only area lights | Tube/capsule closest-point | SIGGRAPH 2014 (Frostbite) | Handles fluorescent tube fixtures naturally |

**Deprecated/outdated:**
- PCSS with Poisson disk (still valid): Preferred over fixed-kernel PCF when penumbra should scale with blocker distance. For this project's aesthetic (uniformly soft shadows), fixed-kernel PCF is simpler and artifact-free.
- HBAO+: Higher quality than SSAO but requires proprietary NVIDIA extensions for full implementation. The open implementation is slower than advertised and adds complexity without visible benefit at "subtle AO strength."

---

## Claude's Discretion Recommendations

Based on research findings, here are the specific recommendations for the discretion areas:

**SSAO Technique:** Classic hemisphere SSAO with 32 samples. Not HBAO (3x cost, marginal quality gain for subtle AO). Not GTAO (7x cost, designed for strong AO effects, not subtle contact darkening).

**Shadow Technique:** 16-tap rotated Poisson PCF for spot lights. Not PCSS — for the art direction goal of "shadows that suggest geometry rather than drawing hard lines," a fixed large-kernel PCF produces more consistent results than PCSS's distance-varying penumbra.

**CSM Cascade Count:** 3 cascades. Split distances: approximately camera_near to 5m (cascade 0), 5m to 20m (cascade 1), 20m to camera_far (cascade 2). Shadow map resolution: 1024 per cascade (3 × 1024x1024 = 3MB depth array).

**Spot Shadow Count:** Increase from 2 to 8. The uniform array in scene.frag is already bounded at 2; extend to 8. Performance impact is linear per shadowed light — 8 shadow passes at 1024 resolution is still fast.

**Bloom Mip Chain Depth:** 5 mip levels. Starting at half resolution (640x360 for 1280x720 viewport). Each level halves. Filter radius: 0.005 in texture space. No threshold — let the HDR downsample do the work.

**Area Light Types:** Implement both AreaRect (LTC, D-09) and Tube (closest-point, D-10). Skip a separate Emissive light type — emissive material property + bloom + optional paired AreaRect/Tube covers the use case with less system complexity.

**Emissive Mesh Lighting (D-11):** Implement as material-level `emissiveStrength` float uniform in scene.frag. Not a light type. Combined with mip-chain bloom, this produces the "overhead lights gently bleed into surroundings" effect described in CONTEXT.md specifics.

---

## Environment Availability

This phase is pure code/config changes to existing C++ and GLSL. All tools confirmed available.

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build system | Yes | 4.1.1 | — |
| Clang++ | C++ compiler | Yes | Apple 17.0.0 | — |
| OpenGL 4.1 (via GLFW) | All rendering | Yes | 4.1 Core (macOS) | — |
| GLSL 4.10 | All shaders | Yes | Confirmed macOS ceiling | — |
| GL_TEXTURE_2D_ARRAY | CSM storage | Yes | OpenGL 3.0+ feature, confirmed 4.1 | N-pass fallback |
| Geometry shaders + invocations | CSM single-pass | Yes | OpenGL 4.1 feature | N separate draw calls |

**No missing dependencies.** All LTC data is embedded as static C++ arrays — no external files to download at runtime.

---

## Open Questions

1. **Geometric vs. perturbed normals for SSAO**
   - What we know: `fragNormal` encodes post-normal-map normals; using these for SSAO hemisphere orientation will produce material-dependent AO (stone blocks would look different from smooth concrete)
   - What's unclear: Whether this looks good or bad in practice for this project's aesthetic
   - Recommendation: Add `layout(location = 2) out vec4 fragGeomNormal` in scene.frag that writes `normalize(vNormal)` unperturbed, and use this for SSAO. Cheap to add, eliminates the issue.

2. **Spot shadow count increase from 2 to 8**
   - What we know: The shader uses explicit `if (shadowIndex == 0) ... else ...` branching for samplers — extending to 8 requires 8-way branching or a different approach
   - What's unclear: Whether dynamic indexing into `sampler2D uShadowMaps[8]` is reliable on macOS OpenGL drivers
   - Recommendation: Use explicit cascaded `if/else` branching for indices 0–7 (verbose but safe). Or use `GL_TEXTURE_2D_ARRAY` for spot shadow maps as well (all same resolution).

3. **LTC table source license**
   - What we know: Eric Heitz's `selfshadow/ltc_code` repository contains the precomputed tables
   - What's unclear: The repository's license for the data tables specifically (the code uses a custom license)
   - Recommendation: Use the WebGL demo tables from [eheitzresearch.wordpress.com](https://eheitzresearch.wordpress.com/415-2/) which are provided for use in any engine, or recompute them using the open-source fitting code.

---

## Sources

### Primary (HIGH confidence)
- [learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom) — bloom mip-chain algorithm, 13-tap downsample, 3x3 tent upsample, confirmed pattern
- [learnopengl.com/Guest-Articles/2021/CSM](https://learnopengl.com/Guest-Articles/2021/CSM) — GL_TEXTURE_2D_ARRAY CSM, geometry shader invocations, frustum splitting, verified
- [learnopengl.com/Advanced-Lighting/SSAO](https://learnopengl.com/Advanced-Lighting/SSAO) — classic SSAO kernel, noise texture, blur pass, confirmed
- [learnopengl.com/Guest-Articles/2022/Area-Lights](https://learnopengl.com/Guest-Articles/2022/Area-Lights) — LTC area lights, LUT format, GLSL patterns, verified
- [Khronos OpenGL Wiki: Array Texture](https://www.khronos.org/opengl/wiki/Array_Texture) — GL_TEXTURE_2D_ARRAY, sampler2DArray GLSL 4.10 compatibility confirmed
- [Khronos OpenGL Wiki: Sampler (GLSL)](https://www.khronos.org/opengl/wiki/Sampler_(GLSL)) — sampler2DArrayShadow availability in GLSL 4.10 confirmed
- Existing engine source code (`scene.frag`, `ShadowMap.h`, `RuntimeSceneRenderer.h/.cpp`, `Framebuffer.h`, `CompositePass.h`) — integration points verified directly

### Secondary (MEDIUM confidence)
- [alextardif.com/arealights.html](https://alextardif.com/arealights.html) — tube light closest-point implementation, PBR integration
- [wickedengine.net/2017/09/area-lights/](https://wickedengine.net/2017/09/area-lights/) — rectangle + tube light approximations, Frostbite paper reference
- [theorangeduck.com/page/pure-depth-ssao](https://theorangeduck.com/page/pure-depth-ssao) — depth-only SSAO, position reconstruction from depth
- [blog.magnum.graphics/guest-posts/area-lights-with-ltcs/](https://blog.magnum.graphics/guest-posts/area-lights-with-ltcs/) — LTC implementation details, confirmed Magnum Engine
- [selfshadow/ltc_code](https://github.com/selfshadow/ltc_code) — reference LTC implementation, precomputed tables, Eric Heitz's original code
- [developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf](https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf) — PCSS blocker search algorithm, penumbra estimation
- [eheitzresearch.wordpress.com/415-2/](https://eheitzresearch.wordpress.com/415-2/) — LTC paper, original precomputed table source

### Tertiary (LOW confidence)
- Performance comparisons between SSAO/HBAO/GTAO — numbers from community benchmarks, not official vendor data; treat as order-of-magnitude estimates, not precise figures
- macOS geometry shader performance characteristics — based on community forum reports, not Apple documentation

---

## Metadata

**Confidence breakdown:**
- Standard Stack: HIGH — all libraries already in use; no new dependencies
- Shadow improvements (PCF, CSM): HIGH — algorithms documented at multiple authoritative sources, OpenGL 4.1 compatibility confirmed
- SSAO: HIGH — classic SSAO is the most documented SSAO variant; depth/normal reconstruction patterns verified
- Bloom mip-chain: HIGH — COD Advanced Warfare technique, documented in detail at LearnOpenGL
- Area lights (LTC): HIGH — published at SIGGRAPH 2016, reference implementation available, tutorial verified
- Tube lights: MEDIUM — closest-point method is from Frostbite 2014 paper, single detailed primary source
- Performance numbers: LOW — macOS OpenGL performance is driver/GPU specific, estimates only

**Research date:** 2026-03-30
**Valid until:** 2026-09-30 (techniques are stable; no major changes expected in this domain)
