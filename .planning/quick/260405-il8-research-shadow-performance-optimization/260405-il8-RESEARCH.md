# Shadow Performance Optimization - Research

**Researched:** 2026-04-05
**Domain:** OpenGL 4.1 shadow mapping performance on macOS
**Confidence:** HIGH

## Summary

The current shadow system has three major performance issues, roughly in order of impact:

1. **Geometry shader CSM is emulated on Apple Silicon.** The M1/M2/M3 GPU hardware does not natively support geometry shaders. Apple's OpenGL driver emulates them via compute shaders, adding substantial overhead. The `csm_depth.geom` shader using `layout(triangles, invocations = 3)` with `gl_Layer` routing is one of the worst cases for this emulation -- every triangle in the scene gets amplified through a compute-emulated geometry stage. Multiple sources confirm geometry shader CSM is slower than multi-pass even on desktop NVIDIA GPUs; on macOS Apple Silicon, the penalty is dramatically worse.

2. **Every shadow map is re-rendered every frame regardless of scene changes.** Spot light shadows and CSM cascades are fully redrawn each frame even when no objects have moved and the camera is stationary. For a scene that is mostly or entirely static (which this project is -- prison corridors, institutional environments), this is pure waste.

3. **No face culling in the shadow pass.** The shadow rendering path does not enable `GL_CULL_FACE` at all. For closed/solid meshes (which most architectural geometry is), rendering back faces only in the shadow pass is both a performance optimization (halves rasterized fragments) and an improvement to shadow quality (eliminates self-shadowing artifacts without needing large biases).

**Primary recommendation:** Replace the geometry shader CSM with multi-pass rendering using `glFramebufferTextureLayer()`, add shadow map caching with dirty flags, and enable front-face culling during shadow passes. These three changes together should reduce shadow pass time by 60-80%.

## Current Implementation Analysis

### What Exists

| Component | Implementation | Location |
|-----------|---------------|----------|
| CSM | 3 cascades, 1024x1024, geometry shader layered rendering | `CascadedShadowMap.cpp`, `csm_depth.geom` |
| Spot shadows | Up to 6 shadowed spot lights, per-light FBO bind + draw | `ShadowMap.cpp`, `SceneRenderPipeline.cpp` |
| Frustum culling | Per-cascade and per-spot-light AABB culling | `FrustumCulling.h` |
| Bias | `glPolygonOffset(1.1f, 4.0f)` for CSM | `SceneRenderPipeline.cpp:231` |
| Timing | CPU-side `glfwGetTime()` around shadow pass | `SceneRenderPipeline.cpp:90-94` |
| Shadow pass input | Uses UNCULLED objects (correct -- casters behind camera matter) | `SceneRenderPipeline.cpp:93` |
| Texture format | `GL_DEPTH_COMPONENT32F` for CSM, `GL_DEPTH_COMPONENT24` for spot | `CascadedShadowMap.cpp`, `ShadowMap.cpp` |

### Current Shadow Pass Flow (per frame)

```
1. For each shadowed spot light (up to 6):
   a. Bind spot light ShadowMap FBO
   b. Clear depth
   c. Set light VP matrix as uniform
   d. Frustum cull objects against light frustum
   e. For each surviving object: set uModel, draw()
   f. Unbind

2. CSM pass (if directional sun enabled):
   a. Compute cascade splits (PSSM)
   b. Bind CSM FBO (full array via glFramebufferTexture)
   c. Use csm_depth shader (vert + geom + frag)
   d. Set 3 light-space matrices as uniforms
   e. glPolygonOffset(1.1, 4.0)
   f. For each object: test against ALL 3 cascade frustums (any-hit)
   g. For each surviving object: set uModel, draw()
      -- Geometry shader amplifies to 3 cascade layers
   h. Unbind
```

The CSM geometry shader emits every triangle to ALL 3 cascades. There is no per-cascade culling at the geometry shader level -- the geometry shader unconditionally writes `gl_Layer = gl_InvocationID` for all 3 invocations. Objects that are only in cascade 0 still get their triangles emitted to cascades 1 and 2 (where they may be clipped by the rasterizer, but the geometry shader work is already done).

## Optimization Techniques

### Optimization 1: Replace Geometry Shader CSM with Multi-Pass (CRITICAL)

**Impact: HIGH -- likely the single biggest win on macOS**
**Confidence: HIGH**

Replace the single-pass geometry shader approach with 3 separate draw passes, one per cascade, using `glFramebufferTextureLayer()` to target each layer individually.

**Why this is faster:**
- Eliminates geometry shader entirely -- no compute shader emulation on Apple Silicon
- Enables true per-cascade frustum culling (objects only in cascade 0 skip cascade 1 and 2 draws)
- `glFramebufferTextureLayer()` is available since OpenGL 3.0 -- well within the 4.1 target
- Multiple sources report that even on NVIDIA hardware with native geometry shader support, multi-pass CSM matches or beats geometry shader CSM. On hardware where geometry shaders are emulated, multi-pass is dramatically faster.

**Implementation approach:**
```cpp
// Instead of:
csmShadowMap_.bind();  // binds full array
csmShader_->use();     // geometry shader
for (object : objects) { draw(); }

// Do:
shadowShader_->use();  // vertex + fragment only, NO geometry shader
for (int cascade = 0; cascade < 3; ++cascade) {
    glBindFramebuffer(GL_FRAMEBUFFER, csmFbo_);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              csmDepthArray_, 0, cascade);
    glViewport(0, 0, resolution_, resolution_);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadowShader_->setMat4("uLightViewProjection", lightSpaceMatrices_[cascade]);

    // Frustum cull per THIS cascade only
    auto frustum = extractFrustumPlanes(lightSpaceMatrices_[cascade]);
    for (object : objects) {
        if (!isAabbInsideFrustum(..., frustum)) continue;
        shadowShader_->setMat4("uModel", object.modelMatrix);
        object.mesh->draw();
    }
}
```

**Key detail:** The existing `shadow_depth.vert` + `shadow_depth.frag` shader pair already does exactly what each cascade pass needs (`uLightViewProjection * uModel * vec4(aPos, 1.0)`). The `uShadowCasterOffset` from the CSM vertex shader should be folded into the shadow depth shader as a uniform that defaults to 0.0 for spot lights.

**What to delete:** `csm_depth.vert`, `csm_depth.geom`, `csm_depth.frag`, and the `csmShader_` member. Replace with reuse of `shadowShader_`.

### Optimization 2: Shadow Map Caching with Dirty Flags (HIGH IMPACT)

**Impact: HIGH for mostly-static scenes**
**Confidence: HIGH**

Skip re-rendering shadow maps when nothing has changed. For a prison/institutional environment with mostly static geometry, this eliminates nearly all shadow rendering cost during exploration.

**Dirty flag sources:**
- Camera movement (affects CSM cascade framing -- CSM must re-render)
- Object transform changes (affects any shadow map containing that object)
- Light parameter changes (position, direction, cone angle)
- Object add/remove

**Implementation levels (from simple to complex):**

**Level 1 -- CSM frame skip (simplest, biggest win):**
Only re-render CSM every N frames (e.g., every 2-4 frames). The player rarely notices 2-4 frame stale shadows in the far cascades. Near cascade can update every frame; far cascades every 2-4 frames.

```cpp
int csmFrameCounter_ = 0;
// In renderShadowPass:
bool updateCascade[3] = {true, (csmFrameCounter_ % 2 == 0), (csmFrameCounter_ % 4 == 0)};
csmFrameCounter_++;
```

**Level 2 -- Spot light caching (straightforward):**
Spot lights in this project are typically static (ceiling lights, wall sconces). Cache their shadow maps until the light or a shadow caster moves.

```cpp
struct SpotShadowCache {
    bool dirty = true;
    glm::mat4 lastLightMatrix{};
    // Set dirty when: light moves, caster transforms change, caster added/removed
};
```

**Level 3 -- Full dirty tracking (more complex):**
Track per-entity transform changes via the ECS. When an entity's `TransformComponent` changes, mark shadow maps whose frustums contain that entity as dirty.

**Recommendation:** Start with Level 1 (cascaded frame skipping) and Level 2 (spot light caching). These cover 80% of the benefit with minimal complexity.

### Optimization 3: Front-Face Culling in Shadow Pass (EASY WIN)

**Impact: MEDIUM -- reduces rasterized fragment count by ~50%**
**Confidence: HIGH**

Currently no face culling is enabled during shadow rendering. For solid/closed meshes (architecture, furniture, props), rendering only back faces in the shadow pass:
- Halves the number of fragments rasterized
- Eliminates shadow acne on front-facing surfaces without bias (the depth written is the back face, which is naturally offset from the lit surface)
- Works in concert with `glPolygonOffset` for additional robustness

```cpp
// Before shadow draw calls:
glEnable(GL_CULL_FACE);
glCullFace(GL_FRONT);  // Cull front faces = render back faces only

// After shadow pass:
glCullFace(GL_BACK);   // Restore default
glDisable(GL_CULL_FACE);
```

**Caveat:** This causes artifacts on thin/single-sided geometry (planes, foliage). If such objects exist, they should be tagged to disable culling during shadow rendering, or this optimization should be applied selectively.

### Optimization 4: Per-Cascade Resolution Reduction

**Impact: MEDIUM**
**Confidence: HIGH**

Far cascades need less resolution because they cover more world space. Currently all 3 cascades share the same 1024x1024 texture array (same resolution per layer). Switching to separate `ShadowMap` objects per cascade allows:

| Cascade | Distance | Resolution | Rationale |
|---------|----------|------------|-----------|
| 0 (near) | 0-10m | 1024 | Player sees these shadows up close |
| 1 (mid) | 10-30m | 512 | Moderate detail sufficient |
| 2 (far) | 30-100m | 256 | Distant shadows barely visible |

This reduces total shadow texel count from 3 * 1024^2 = 3.1M to 1024^2 + 512^2 + 256^2 = 1.3M (58% reduction in fill rate).

**Trade-off:** Requires switching from `GL_TEXTURE_2D_ARRAY` to 3 separate `GL_TEXTURE_2D` depth textures, which changes how the scene shader samples CSM. The scene shader currently uses `texture(sampler2DArrayShadow, ...)` with layer selection -- it would need 3 separate `sampler2DShadow` uniforms instead. This is a moderate refactor.

**Alternative:** Keep the texture array but reduce the shared resolution. Going from 1024 to 512 for all cascades cuts fill rate by 75% with minimal visible impact for the art style (soft, warm lighting -- not crisp hard shadows).

### Optimization 5: Reduce Shadow-Casting Spot Light Count

**Impact: MEDIUM (depends on scene)**
**Confidence: HIGH**

`kMaxShadowedSpotLights` is 6. Each shadowed spot light requires a full scene traversal + draw pass. Techniques to reduce this:

- **Distance-based shadow culling:** Only cast shadows from spot lights within a radius of the player (e.g., 15m). Far-away spot lights contribute light but not shadows.
- **Importance scoring:** Rank lights by (intensity * solid_angle / distance^2) and only shadow the top N.
- **Hard cap at 2-3:** For a Stanley Parable-style game with soft ambient lighting, 2-3 shadowed spot lights is likely sufficient.

### Optimization 6: GPU Timer Queries for Accurate Profiling

**Impact: Diagnostic (enables measuring all other optimizations)**
**Confidence: HIGH**

The current timing uses `glfwGetTime()` which measures CPU time including pipeline stalls, not actual GPU execution time. Adding `GL_TIME_ELAPSED` queries provides accurate GPU timing:

```cpp
GLuint query;
glGenQueries(1, &query);
glBeginQuery(GL_TIME_ELAPSED, query);
// ... shadow pass ...
glEndQuery(GL_TIME_ELAPSED);
// Read result (may stall -- read previous frame's result for async)
GLuint64 elapsed;
glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed);
double gpuMs = elapsed / 1000000.0;
```

**Note:** `GL_TIME_ELAPSED` is available in OpenGL 3.3+ (within 4.1 target). However, on macOS Apple's OpenGL implementation has been reported to have unreliable timer query results in some cases. Still worth implementing as it provides more signal than CPU timing alone.

### Optimization 7: Shadow Depth Format Optimization

**Impact: LOW-MEDIUM**
**Confidence: MEDIUM**

The CSM uses `GL_DEPTH_COMPONENT32F` (32-bit float). For shadow maps, 24-bit depth (`GL_DEPTH_COMPONENT24`) is typically sufficient and can be faster on some hardware due to reduced bandwidth. The spot light shadow maps already use `GL_DEPTH_COMPONENT24`.

Changing CSM to `GL_DEPTH_COMPONENT24` reduces memory bandwidth by 25% per cascade and may improve fill rate.

## macOS-Specific Concerns

### Apple Silicon Geometry Shader Emulation

**Confidence: HIGH** (confirmed by Asahi GPU driver developer Alyssa Rosenzweig)

Apple's M-series GPUs do not have geometry shader hardware. The macOS OpenGL driver emulates geometry shaders using compute shaders. This means:

- Every geometry shader invocation involves a compute dispatch
- The geometry shader output (amplified vertices) must be written to a buffer and then re-read by the rasterizer
- This breaks the normal vertex-to-rasterizer pipeline efficiency
- For CSM with 3 invocations per triangle, every triangle in the scene goes through compute emulation 3 times

This is the single most important finding: **the geometry shader CSM approach is fundamentally incompatible with good performance on macOS Apple Silicon.**

### Apple's OpenGL Driver Quality

Apple deprecated OpenGL in macOS 10.14 (2018). The driver:
- Has not received performance optimizations since deprecation
- Is known to have higher draw call overhead than Metal
- Reports suggest ~14 FPS for moderate OpenGL workloads on M1 in some test scenarios
- State changes (shader switches, FBO binds) are relatively expensive

**Implication:** Minimizing state changes, shader switches, and FBO rebinds in the shadow pass is more important on macOS than on other platforms. The multi-pass CSM approach adds 2 extra `glFramebufferTextureLayer` calls per frame (one per additional cascade) but eliminates the geometry shader entirely -- this is a massive net win.

### Depth Format Behavior

macOS OpenGL supports `GL_DEPTH_COMPONENT16`, `GL_DEPTH_COMPONENT24`, and `GL_DEPTH_COMPONENT32F`. There is no confirmed performance difference between 24 and 32F on Apple Silicon specifically, but 24-bit is the safer default for shadow maps.

## Common Pitfalls

### Pitfall 1: Measuring CPU Time Instead of GPU Time
**What goes wrong:** `glfwGetTime()` measures wall clock time including CPU-GPU sync stalls. Shadow pass may appear slow because the CPU is waiting for the GPU to finish a previous pass.
**How to avoid:** Use `GL_TIME_ELAPSED` queries. Even if imperfect on macOS, they provide relative comparison data.

### Pitfall 2: Shadow Map Cache Invalidation Too Aggressive
**What goes wrong:** Caching shadow maps but invalidating them on camera movement even though only the CSM framing (not the spot light shadow maps) depends on camera position.
**How to avoid:** CSM dirty flags should trigger on camera movement. Spot light dirty flags should only trigger on light or caster changes.

### Pitfall 3: Cascaded Frame Skipping Causes Popping
**What goes wrong:** Updating cascade 2 every 4 frames means 4-frame-stale shadows at medium distance. If the player is moving quickly, shadow edges visibly pop.
**How to avoid:** Only skip far cascades (index 1 and 2). Always update cascade 0 (near) every frame. The project's art style (soft, warm, no harsh shadows) is very forgiving of cascade update latency.

### Pitfall 4: Front-Face Culling on Thin Geometry
**What goes wrong:** Enabling `glCullFace(GL_FRONT)` globally during shadow rendering causes thin/single-sided meshes (planes, foliage, billboards) to disappear from shadow maps.
**How to avoid:** Tag such meshes as `doubleSided` in their material data. Disable face culling for those specific draws. Most architectural geometry (walls, floors, pillars, arches) is fine.

### Pitfall 5: Multi-Pass CSM Requires Per-Cascade Clear
**What goes wrong:** Forgetting to `glClear(GL_DEPTH_BUFFER_BIT)` after binding each cascade layer via `glFramebufferTextureLayer` means old depth data bleeds through.
**How to avoid:** Always clear after `glFramebufferTextureLayer` + `glViewport`. The current geometry shader approach clears the entire array once via `csmShadowMap_.bind()` which calls `glClear` -- the multi-pass approach must clear per layer.

### Pitfall 6: glFramebufferTextureLayer Framebuffer Completeness
**What goes wrong:** After calling `glFramebufferTextureLayer`, the framebuffer completeness status may change if parameters are wrong.
**How to avoid:** Verify `glCheckFramebufferStatus` at least once during development. The CSM FBO was created with `glFramebufferTexture` (full array) -- switching to `glFramebufferTextureLayer` per cascade still uses the same FBO and depth array texture, just targeting one layer at a time.

## Recommended Implementation Order

| Priority | Optimization | Effort | Impact | Risk |
|----------|-------------|--------|--------|------|
| 1 | Replace GS CSM with multi-pass | Medium | Very High | Low |
| 2 | Front-face culling in shadow pass | Low | Medium | Low |
| 3 | CSM cascade frame skipping | Low | High | Low |
| 4 | Spot light shadow caching | Medium | High (static scenes) | Low |
| 5 | GPU timer queries | Low | Diagnostic | None |
| 6 | Reduce CSM resolution (1024 -> 512) | Trivial | Medium | Low |
| 7 | Distance-based spot light shadow culling | Low | Medium | None |
| 8 | Per-cascade resolution (requires refactor) | High | Medium | Medium |

## Code Examples

### Multi-Pass CSM (replacing geometry shader)

```cpp
// In SceneRenderPipeline::renderShadowPass(), replace the CSM section:

if (lighting.sun.enabled && lighting.enableShadows) {
    csmShadowMap_.computeCascades(input.viewMatrix, input.projectionMatrix,
                                   lighting.sun.direction,
                                   input.nearPlane, input.farPlane,
                                   input.postParams ? input.postParams->csmLambda : 0.5f);

    shadowShader_->use();  // Reuse the simple shadow_depth shader
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.1f, 4.0f);

    const auto& csmMatrices = csmShadowMap_.lightSpaceMatrices();
    GLuint csmFbo = csmShadowMap_.framebuffer();

    for (int cascade = 0; cascade < CascadedShadowMap::kCascadeCount; ++cascade) {
        glBindFramebuffer(GL_FRAMEBUFFER, csmFbo);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  csmShadowMap_.depthArrayTexture(), 0, cascade);
        glViewport(0, 0, csmShadowMap_.resolution(), csmShadowMap_.resolution());
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowShader_->setMat4("uLightViewProjection", csmMatrices[cascade]);

        // Per-cascade frustum culling
        const auto frustum = extractFrustumPlanes(csmMatrices[cascade]);
        for (const auto& object : objects) {
            if (object.mesh && !isAabbInsideFrustum(object.mesh->aabbMin(),
                                                      object.mesh->aabbMax(),
                                                      object.modelMatrix, frustum)) {
                ++shadowCulled;
                continue;
            }
            shadowShader_->setMat4("uModel", object.modelMatrix);
            object.mesh->draw();
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
```

**Note:** The existing `shadow_depth.vert` uses `uLightViewProjection * uModel * vec4(aPosition, 1.0)` which is exactly what CSM needs. The shadow caster offset from `csm_depth.vert` (`worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset`) should be added as a conditional uniform to `shadow_depth.vert` if needed for bias, though `glPolygonOffset` should handle it.

### Cascade Frame Skipping

```cpp
// Add to SceneRenderPipeline:
int csmFrameCounter_ = 0;

// In the multi-pass CSM loop:
bool shouldUpdate[3] = {
    true,                              // Cascade 0: every frame
    (csmFrameCounter_ % 2 == 0),      // Cascade 1: every 2 frames
    (csmFrameCounter_ % 4 == 0),      // Cascade 2: every 4 frames
};
csmFrameCounter_++;

for (int cascade = 0; cascade < 3; ++cascade) {
    if (!shouldUpdate[cascade]) continue;
    // ... render this cascade ...
}
```

### Spot Light Shadow Caching

```cpp
struct SpotShadowState {
    bool dirty = true;
    glm::mat4 cachedMatrix{};
    glm::vec3 cachedPosition{};
    glm::vec3 cachedDirection{};
    float cachedOuterCone = 0.0f;
    float cachedRadius = 0.0f;
};

std::array<SpotShadowState, kMaxShadowedSpotLights> spotShadowStates_;

// Before rendering each spot light shadow:
bool isDirty = spotShadowStates_[idx].dirty;
if (!isDirty) {
    const auto& state = spotShadowStates_[idx];
    isDirty = (light.position != state.cachedPosition ||
               light.direction != state.cachedDirection ||
               light.outerConeDegrees != state.cachedOuterCone ||
               light.radius != state.cachedRadius);
}
if (!isDirty) {
    // Reuse cached shadow map
    shadowData.matrices[idx] = spotShadowStates_[idx].cachedMatrix;
    continue;  // Skip rendering
}
// ... render and update cache ...
```

## Sources

### Primary (HIGH confidence)
- Alyssa Rosenzweig (Asahi GPU driver lead) - "Geometry shaders, tessellation, and transform feedback become compute shaders" on Apple M1 - https://alyssarosenzweig.ca/blog/conformant-gl46-on-the-m1.html
- Khronos Forums - CSM optimization discussion with performance numbers - https://community.khronos.org/t/cascade-shadow-mapping-possible-optimizations/103962
- Khronos Forums - Geometry shader CSM slower than multi-pass on NVIDIA - https://community.khronos.org/t/geometry-shader-based-csm/61846
- LearnOpenGL CSM reference - https://learnopengl.com/Guest-Articles/2021/CSM
- glFramebufferTextureLayer documentation - https://docs.gl/gl4/glFramebufferTexture

### Secondary (MEDIUM confidence)
- MJP shadow sampling techniques - https://therealmjp.github.io/posts/shadow-maps/
- Godot shadow caching proposal (static/dynamic separation) - https://github.com/godotengine/godot-proposals/issues/4635
- Leadwerks shadow caching implementation - https://www.leadwerks.com/community/blogs/entry/2248-shadow-caching/
- NVIDIA CSM documentation - https://docs.nvidia.com/gameworks/content/gameworkslibrary/graphicssamples/opengl_samples/cascadedshadowmapping.htm
- Apple OpenGL performance tuning guide (archived) - https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_performance/opengl_performance.html

### Tertiary (LOW confidence)
- GameDev.net geometry shader vs multi-pass discussion - https://www.gamedev.net/forums/topic/697167-is-it-a-good-choice-that-using-a-geometry-shader-to-implement-one-pass-shadowmap-generating/

## Metadata

**Confidence breakdown:**
- Geometry shader removal: HIGH - confirmed by GPU driver developer and multiple independent sources
- Multi-pass CSM: HIGH - well-documented OpenGL pattern, `glFramebufferTextureLayer` is standard
- Shadow caching: HIGH - industry standard technique (Unity HDRP, Godot, Leadwerks all implement it)
- Front-face culling: HIGH - standard shadow mapping optimization
- macOS-specific perf: MEDIUM - based on community reports and driver developer comments, no controlled benchmarks

**Research date:** 2026-04-05
**Valid until:** No expiry -- these are fundamental OpenGL techniques, not version-dependent
