---
phase: quick
plan: 260405-isu
type: execute
wave: 1
depends_on: []
files_modified:
  - src/engine/rendering/SceneRenderPipeline.cpp
  - src/engine/rendering/SceneRenderPipeline.h
  - src/engine/rendering/lighting/CascadedShadowMap.h
  - src/engine/rendering/lighting/CascadedShadowMap.cpp
autonomous: true
---

<objective>
Replace the geometry shader CSM approach with multi-pass rendering. Each cascade is rendered in a separate pass using `glFramebufferTextureLayer()` to target individual layers of the existing depth array texture. Reuses the existing `shadow_depth` shader. Deletes the `csm_depth` geometry shader pipeline. Also adds front-face culling to the shadow pass as a free win.

This eliminates the geometry shader entirely — critical on macOS where Apple Silicon emulates geometry shaders via compute shaders, causing massive overhead.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/engine/rendering/SceneRenderPipeline.cpp
@src/engine/rendering/SceneRenderPipeline.h
@src/engine/rendering/lighting/CascadedShadowMap.h
@src/engine/rendering/lighting/CascadedShadowMap.cpp
@assets/shaders/engine/shadow_depth.vert
@assets/shaders/engine/shadow_depth.frag
@assets/shaders/engine/csm_depth.vert
@assets/shaders/engine/csm_depth.geom
@assets/shaders/engine/csm_depth.frag

<interfaces>
The existing `shadow_depth.vert` uses `uLightViewProjection * uModel * vec4(aPosition, 1.0)` — exactly what each CSM cascade pass needs.

CascadedShadowMap already exposes:
- `framebuffer()` — the FBO
- `depthArrayTexture()` — the GL_TEXTURE_2D_ARRAY
- `resolution()` — resolution per cascade
- `lightSpaceMatrices()` — array of 3 matrices
- `computeCascades()` — PSSM split computation

The FBO was created with `glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray_, 0)` which attaches all layers. For multi-pass, we use `glFramebufferTextureLayer()` to target individual layers.

CascadedShadowMap::bind() currently clears the entire array at once. For multi-pass, we need per-layer clear, so we won't use bind() — we'll bind the FBO and set up each layer manually.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Replace geometry shader CSM with multi-pass and add front-face culling</name>
  <files>src/engine/rendering/SceneRenderPipeline.cpp, src/engine/rendering/SceneRenderPipeline.h</files>
  <action>
1. In `SceneRenderPipeline.h`:
   - Remove the `std::unique_ptr<Shader> csmShader_;` member (line 119)

2. In `SceneRenderPipeline.cpp`:
   - In `init()`: Remove the 3 lines that create `csmShader_` (lines 28-30: `csmShader_ = std::make_unique<Shader>("assets/shaders/engine/csm_depth.vert", ...`)

   - In `renderShadowPass()`: Replace the entire CSM section (lines 204-255). The CSM condition should check `shadowShader_` instead of `csmShader_`:

     Replace:
     ```cpp
     // Render CSM for directional sun
     const LightingEnvironment& lighting = input.lightingEnvironment;
     if (lighting.sun.enabled && lighting.enableShadows && csmShader_ != nullptr) {
         csmShadowMap_.computeCascades(input.viewMatrix,
                                        input.projectionMatrix,
                                        lighting.sun.direction,
                                        input.nearPlane,
                                        input.farPlane,
                                        input.postParams ? input.postParams->csmLambda : 0.5f);

         csmShadowMap_.bind();
         csmShader_->use();
         csmShader_->setVec3("uLightDirection", lighting.sun.direction);
         csmShader_->setFloat("uShadowCasterOffset", 0.18f);

         const auto& csmMatrices = csmShadowMap_.lightSpaceMatrices();
         for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
             csmShader_->setMat4("uLightSpaceMatrices[" + std::to_string(i) + "]", csmMatrices[i]);
         }

         // Build frustum planes for each cascade
         std::array<std::array<glm::vec4, 6>, CascadedShadowMap::kCascadeCount> cascadeFrustums;
         for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
             cascadeFrustums[i] = extractFrustumPlanes(csmMatrices[i]);
         }

         glEnable(GL_POLYGON_OFFSET_FILL);
         glPolygonOffset(1.1f, 4.0f);

         for (const auto& object : objects) {
             if (object.mesh) {
                 bool inAnyCascade = false;
                 for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
                     if (isAabbInsideFrustum(object.mesh->aabbMin(), object.mesh->aabbMax(),
                                              object.modelMatrix, cascadeFrustums[i])) {
                         inAnyCascade = true;
                         break;
                     }
                 }
                 if (!inAnyCascade) {
                     ++shadowCulled;
                     continue;
                 }
             }
             csmShader_->setMat4("uModel", object.modelMatrix);
             object.mesh->draw();
         }

         glDisable(GL_POLYGON_OFFSET_FILL);

         csmShadowMap_.unbind();
     }
     ```

     With:
     ```cpp
     // Render CSM for directional sun — multi-pass, one pass per cascade
     const LightingEnvironment& lighting = input.lightingEnvironment;
     if (lighting.sun.enabled && lighting.enableShadows && shadowShader_ != nullptr) {
         csmShadowMap_.computeCascades(input.viewMatrix,
                                        input.projectionMatrix,
                                        lighting.sun.direction,
                                        input.nearPlane,
                                        input.farPlane,
                                        input.postParams ? input.postParams->csmLambda : 0.5f);

         shadowShader_->use();
         glEnable(GL_POLYGON_OFFSET_FILL);
         glPolygonOffset(1.1f, 4.0f);
         glEnable(GL_CULL_FACE);
         glCullFace(GL_FRONT);

         const auto& csmMatrices = csmShadowMap_.lightSpaceMatrices();
         const int csmRes = csmShadowMap_.resolution();

         for (int cascade = 0; cascade < CascadedShadowMap::kCascadeCount; ++cascade) {
             glBindFramebuffer(GL_FRAMEBUFFER, csmShadowMap_.framebuffer());
             glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                       csmShadowMap_.depthArrayTexture(), 0, cascade);
             glViewport(0, 0, csmRes, csmRes);
             glClear(GL_DEPTH_BUFFER_BIT);

             shadowShader_->setMat4("uLightViewProjection", csmMatrices[cascade]);

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

     Key changes:
     - Uses `shadowShader_` (existing shadow_depth shader) instead of `csmShader_`
     - Loop per cascade: `glFramebufferTextureLayer` targets one layer, clear, set that cascade's matrix, frustum cull per cascade, draw
     - Adds `glCullFace(GL_FRONT)` for front-face culling (halves rasterized fragments)
     - Per-cascade frustum culling means objects only in cascade 0 skip cascades 1 and 2
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor pixel-roguelike 2>&1 | tail -5</automated>
  </verify>
  <done>CSM uses multi-pass rendering with shadow_depth shader, no geometry shader. Front-face culling enabled during shadow pass. Per-cascade frustum culling active. csmShader_ member removed.</done>
</task>

<task type="auto">
  <name>Task 2: Delete geometry shader files and clean up CascadedShadowMap</name>
  <files>assets/shaders/engine/csm_depth.vert, assets/shaders/engine/csm_depth.geom, assets/shaders/engine/csm_depth.frag, src/engine/rendering/lighting/CascadedShadowMap.cpp</files>
  <action>
1. Delete the 3 geometry shader CSM files:
   - `assets/shaders/engine/csm_depth.vert`
   - `assets/shaders/engine/csm_depth.geom`
   - `assets/shaders/engine/csm_depth.frag`

2. In `CascadedShadowMap.cpp`, update the `bind()` method. The current `bind()` calls `glClear(GL_DEPTH_BUFFER_BIT)` which cleared all layers at once when the full array was attached. This is no longer used by the pipeline (multi-pass clears per layer), but keep `bind()` functional for any other callers — just remove the clear since it would only clear the currently-attached layer:

   Change `bind()` from:
   ```cpp
   void CascadedShadowMap::bind() const {
       glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
       glViewport(0, 0, resolution_, resolution_);
       glClear(GL_DEPTH_BUFFER_BIT);
   }
   ```
   To:
   ```cpp
   void CascadedShadowMap::bind() const {
       glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
       glViewport(0, 0, resolution_, resolution_);
   }
   ```

3. In `CascadedShadowMap.cpp`, in `create()`, change the initial framebuffer attachment from `glFramebufferTexture` (full array) to `glFramebufferTextureLayer` targeting layer 0. This makes the FBO valid immediately for the multi-pass approach:

   Replace:
   ```cpp
   glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray_, 0);
   ```
   With:
   ```cpp
   glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray_, 0, 0);
   ```
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor pixel-roguelike 2>&1 | tail -5 && ls assets/shaders/engine/csm_depth.* 2>&1</automated>
  </verify>
  <done>Geometry shader files deleted. CascadedShadowMap updated for per-layer attachment. Build passes.</done>
</task>

</tasks>

<verification>
1. Build succeeds for level-editor and pixel-roguelike
2. The 3 csm_depth.* shader files no longer exist
3. Launch editor, enter play preview — CSM shadows still render correctly
4. Performance panel should show significant reduction in shadow pass time
</verification>

<output>
After completion, create `.planning/quick/260405-isu-replace-geometry-shader-csm-with-multi-p/260405-isu-SUMMARY.md`
</output>
