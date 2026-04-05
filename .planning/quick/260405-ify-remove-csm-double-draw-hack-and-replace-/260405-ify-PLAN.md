---
phase: quick
plan: 260405-ify
type: execute
wave: 1
depends_on: []
files_modified:
  - src/engine/rendering/SceneRenderPipeline.cpp
  - assets/shaders/engine/csm_depth.vert
autonomous: true
---

<objective>
Remove the CSM double-draw hack that renders every shadow caster twice (once with +0.03 normal offset, once with -0.03) and replace it with standard GPU polygon offset bias. This halves CSM draw calls.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/engine/rendering/SceneRenderPipeline.cpp
@assets/shaders/engine/csm_depth.vert
@assets/shaders/engine/csm_depth.geom
@assets/shaders/engine/csm_depth.frag

<interfaces>
The current CSM rendering in SceneRenderPipeline.cpp lines 204-254 does:
1. computeCascades(), bind CSM FBO, set shader uniforms
2. For each object: draw with uShadowNormalOffset=+0.03, then draw again with -0.03
3. Unbind

The double-draw creates a "thickened" shadow shell to prevent light leaking. The proper alternative is `glEnable(GL_POLYGON_OFFSET_FILL)` + `glPolygonOffset(factor, units)` which biases depth at the GPU level — zero extra draw calls.

The vertex shader (csm_depth.vert) currently applies the normal offset:
```glsl
worldPos.xyz += worldNormal * uShadowNormalOffset;
```

The `uShadowCasterOffset` (light direction push of 0.18) is separate and should be KEPT — it prevents peter-panning by pushing casters slightly toward the light.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Replace CSM double-draw with glPolygonOffset</name>
  <files>src/engine/rendering/SceneRenderPipeline.cpp, assets/shaders/engine/csm_depth.vert</files>
  <action>
1. In `assets/shaders/engine/csm_depth.vert`:
   - Remove the `uniform float uShadowNormalOffset;` line
   - Remove the `worldPos.xyz += worldNormal * uShadowNormalOffset;` line
   - Remove the `layout(location = 1) in vec3 aNormal;` input since it's no longer needed
   - Keep `uShadowCasterOffset` and the light direction offset — that stays

   The shader should become:
   ```glsl
   #version 410 core
   layout(location = 0) in vec3 aPos;
   uniform mat4 uModel;
   uniform vec3 uLightDirection;
   uniform float uShadowCasterOffset;
   void main() {
       vec4 worldPos = uModel * vec4(aPos, 1.0);
       worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
       gl_Position = worldPos;
   }
   ```

2. In `src/engine/rendering/SceneRenderPipeline.cpp`, in the `renderShadowPass()` method, replace the CSM section's draw loop. Find the block starting at the `for (const auto& object : objects)` loop inside the CSM section (after the cascade frustum building).

   Replace:
   ```cpp
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
       csmShader_->setFloat("uShadowNormalOffset", 0.03f);
       object.mesh->draw();
       csmShader_->setFloat("uShadowNormalOffset", -0.03f);
       object.mesh->draw();
   }
   ```

   With:
   ```cpp
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
   ```

   The values `glPolygonOffset(1.1f, 4.0f)` are standard shadow mapping bias values. Factor=1.1 scales the depth slope, units=4.0 adds a constant offset. This prevents shadow acne without the double-draw.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor pixel-roguelike 2>&1 | tail -5</automated>
  </verify>
  <done>CSM pass draws each shadow caster once instead of twice. glPolygonOffset handles shadow acne bias at the GPU level. Shader no longer needs normal attribute or normal offset uniform.</done>
</task>

</tasks>

<verification>
1. Build succeeds for level-editor and pixel-roguelike
2. Launch editor, enter play preview — shadows still render correctly without visible acne
3. Performance panel should show ~50% reduction in shadow pass time
</verification>

<output>
After completion, create `.planning/quick/260405-ify-remove-csm-double-draw-hack-and-replace-/260405-ify-SUMMARY.md`
</output>
