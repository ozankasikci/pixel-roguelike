---
phase: quick
plan: 260402-dec
type: execute
wave: 1
depends_on: []
files_modified:
  - src/engine/rendering/SceneRenderPipeline.h
  - src/engine/rendering/SceneRenderPipeline.cpp
  - src/engine/rendering/geometry/Renderer.h
  - src/engine/rendering/geometry/Renderer.cpp
autonomous: true
must_haves:
  truths:
    - "Objects outside a spot light's frustum are not drawn into that light's shadow map"
    - "Objects outside ALL CSM cascade frustums are not drawn into the CSM pass"
    - "Consecutive objects sharing the same material skip redundant texture binds and uniform calls"
    - "Shadow culled count is visible in the render stats overlay"
  artifacts:
    - path: "src/engine/rendering/SceneRenderPipeline.cpp"
      provides: "Shadow frustum culling in renderShadowPass, sort-by-material before scene pass"
    - path: "src/engine/rendering/SceneRenderPipeline.h"
      provides: "shadowCulledCount in SceneRenderPipelineStats"
    - path: "src/engine/rendering/geometry/Renderer.cpp"
      provides: "Redundant state elimination in drawScene loop"
  key_links:
    - from: "SceneRenderPipeline::renderShadowPass"
      to: "FrustumCulling.h"
      via: "extractFrustumPlanes + isAabbInsideFrustum with light VP matrix"
      pattern: "extractFrustumPlanes.*lightMatrix"
    - from: "SceneRenderPipeline::render"
      to: "Renderer::drawScene"
      via: "std::sort on culledObjects before passing to drawScene"
      pattern: "std::sort.*culledObjects"
    - from: "Renderer::drawScene"
      to: "RenderMaterialData::id"
      via: "lastMaterialId tracking to skip redundant binds"
      pattern: "lastMaterialId"
---

<objective>
Shadow frustum culling and render state sorting for performance.

Purpose: Reduce GPU work by (1) culling objects that cannot cast shadows through a given light, and (2) minimizing redundant OpenGL state changes by sorting objects by material before the scene draw pass.

Output: Modified SceneRenderPipeline with per-light shadow culling and pre-sorted draw order; Renderer with redundant state skip logic; updated stats overlay showing shadow cull count.
</objective>

<execution_context>
@.claude/get-shit-done/workflows/execute-plan.md
@.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/engine/rendering/SceneRenderPipeline.h
@src/engine/rendering/SceneRenderPipeline.cpp
@src/engine/rendering/geometry/Renderer.h
@src/engine/rendering/geometry/Renderer.cpp
@src/engine/rendering/FrustumCulling.h
@src/engine/rendering/lighting/CascadedShadowMap.h
</context>

<tasks>

<task type="auto">
  <name>Task 1: Shadow frustum culling for spot lights and CSM</name>
  <files>src/engine/rendering/SceneRenderPipeline.h, src/engine/rendering/SceneRenderPipeline.cpp</files>
  <action>
1. In SceneRenderPipelineStats (SceneRenderPipeline.h), add:
   - `int shadowCulledCount = 0;` — total objects culled across all shadow passes

2. In SceneRenderPipeline::renderShadowPass (SceneRenderPipeline.cpp):

   **Spot light shadow culling (lines 172-175):** Replace the unconditional loop with frustum-culled rendering. For each spot light, `lightMatrix` is already computed at line 162. Extract frustum planes from it and test each object:
   ```cpp
   const auto lightFrustum = extractFrustumPlanes(lightMatrix);
   for (const auto& object : objects) {
       if (object.mesh && !isAabbInsideFrustum(object.mesh->aabbMin(), object.mesh->aabbMax(),
                                                 object.modelMatrix, lightFrustum)) {
           ++shadowCulled;
           continue;
       }
       shadowShader_->setMat4("uModel", object.modelMatrix);
       object.mesh->draw();
   }
   ```
   Declare `int shadowCulled = 0;` at the top of renderShadowPass, before the spot light loop.

   **CSM culling (lines 198-201):** For each object, test against ALL cascade frustums. Only skip if outside every cascade (conservative approach — visible in any cascade means render):
   ```cpp
   // Build frustum planes for each cascade
   std::array<std::array<glm::vec4, 6>, CascadedShadowMap::kCascadeCount> cascadeFrustums;
   for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
       cascadeFrustums[i] = extractFrustumPlanes(csmMatrices[i]);
   }

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
   ```

3. Pass shadowCulled count back to stats. renderShadowPass currently has void return. Two options:
   - Add an `int& outShadowCulled` parameter to renderShadowPass, OR
   - Store directly to `lastStats_.shadowCulledCount` since it is a member.
   Use the simpler approach: write `lastStats_.shadowCulledCount = shadowCulled;` at the end of renderShadowPass (before the method returns). This works because lastStats_ is reset each frame in render().

4. Initialize `lastStats_.shadowCulledCount = 0;` in the render() method alongside the other stat assignments (around line 89-95).
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>Spot light shadow maps and CSM pass skip objects outside the respective light frustums. shadowCulledCount stat is populated per frame.</done>
</task>

<task type="auto">
  <name>Task 2: Render state sorting and redundant bind elimination</name>
  <files>src/engine/rendering/SceneRenderPipeline.cpp, src/engine/rendering/geometry/Renderer.cpp</files>
  <action>
1. **Sort by material in SceneRenderPipeline::render()** — After frustum culling builds `culledObjects` (line 56-65) and before the scene pass, sort the culled objects by material ID then front-to-back distance:
   ```cpp
   // Sort: group by material, then front-to-back within each group
   const glm::vec3 camPos = input.cameraPosition;
   const glm::vec3 forward = glm::normalize(glm::vec3(input.viewMatrix[0][2],
                                                         input.viewMatrix[1][2],
                                                         input.viewMatrix[2][2]));
   std::sort(culledObjects.begin(), culledObjects.end(),
       [&](const RenderObject& a, const RenderObject& b) {
           if (a.material.id != b.material.id) return a.material.id < b.material.id;
           float distA = glm::dot(camPos - glm::vec3(a.modelMatrix[3]), forward);
           float distB = glm::dot(camPos - glm::vec3(b.modelMatrix[3]), forward);
           return distA < distB;
       });
   ```
   Place this AFTER the culling loop and BEFORE the `culledInput.objects = &culledObjects;` assignment (line 68-69).

2. **Redundant state elimination in Renderer::drawScene()** — Track last-bound material to skip redundant uniform and texture calls. In the per-object loop (starting at line 72 of Renderer.cpp):

   Before the loop, declare:
   ```cpp
   std::string lastMaterialId;
   ```

   Inside the loop, after the wireframe/depth handling (lines 73-83), wrap the material uniform block (lines 85-118) in a conditional:
   ```cpp
   const bool sameMaterial = (!obj.material.id.empty() && obj.material.id == lastMaterialId);
   if (!sameMaterial) {
       lastMaterialId = obj.material.id;
       // ... all the existing material uniform + texture bind code (lines 85-118) ...
   }
   ```
   The per-object uniforms that always differ (uModel at line 107, uBaseColor at line 108 since it includes obj.tint) MUST remain OUTSIDE the conditional — they are set every iteration.

   Important: `uBaseColor` combines `obj.tint * material.baseColor`. If tint varies per object but material is the same, the full product must still be set. Keep `setVec3("uBaseColor", ...)` outside the conditional.

   Also keep `setInt("uUnlit", ...)` outside the conditional since it is per-object (obj.unlit), not per-material.

3. Add `int stateSkipCount = 0;` tracking — not exposed in stats (internal optimization), but add a comment noting the optimization for future profiling.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>Objects are sorted by material ID before scene draw. Consecutive objects with the same material skip ~30 redundant glUniform and 4 redundant glBindTexture calls. Per-object uniforms (model matrix, base color, unlit) are always set.</done>
</task>

</tasks>

<verification>
1. Build succeeds for all targets: `cmake --build build --target pixel-roguelike level-editor procedural-model-viewer`
2. Launch level-editor, open a scene with multiple lights — verify no visual regression (objects still cast shadows correctly)
3. Check stats overlay: shadowCulledCount shows non-zero when lights are present and some objects are outside light frustums
4. Check stats overlay: drawCalls and render times should show improvement (fewer shadow draws, fewer state changes)
</verification>

<success_criteria>
- Both executables compile without warnings related to the changed files
- Shadow maps render correctly (no missing shadows for objects that should cast them)
- shadowCulledCount stat reports non-zero values in scenes with spot lights
- No visual artifacts from material sort order (depth test handles draw order correctness)
- Render time shows measurable improvement in scenes with 50+ objects and multiple shadow-casting lights
</success_criteria>

<output>
After completion, create `.planning/quick/260402-dec-shadow-frustum-culling-and-render-state-/260402-dec-SUMMARY.md`
</output>
