---
status: awaiting_human_verify
trigger: "White line artifact along wall-ceiling edges in shadow rendering after replacing geometry shader CSM with multi-pass rendering."
created: 2026-04-05T00:00:00Z
updated: 2026-04-05T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED. The old csm_depth.vert pushed shadow casters 0.18 units toward the light (uShadowCasterOffset). The new multi-pass approach reused shadow_depth.vert which had NO such offset — leaving a coverage gap at grazing-angle junctions that rendered as a bright line.
test: Rebuilt level-editor with fix applied — clean build.
expecting: White line at wall-ceiling junctions is gone in play preview.
next_action: User verifies the artifact is gone in play preview (Cmd+P on initial_scene)

## Symptoms

expected: Clean shadow transitions at wall-ceiling junctions — no bright line artifacts
actual: A bright/white line appears along the top edge of walls where they meet the ceiling. This is visible in play preview mode.
errors: No errors — purely a visual artifact
reproduction: Launch level-editor, open initial_scene, enter play preview (Cmd+P), look at top edge of any wall where it meets ceiling. White/bright line visible.
started: After commit b87ee0f (replaced geometry shader CSM with multi-pass rendering using glFramebufferTextureLayer)

## Eliminated

- hypothesis: Front-face culling is causing the artifact
  evidence: User already tried removing front-face culling — it did not help. The commit log confirms this was added as a "free ~50% rasterization win" and its removal was already attempted.
  timestamp: 2026-04-05T00:00:00Z

## Evidence

- timestamp: 2026-04-05T00:00:00Z
  checked: git diff of commit b87ee0f (the introducing commit)
  found: The old csm_depth.vert had `worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;` with uShadowCasterOffset = 0.18f set from C++. The new shadow_depth.vert is `gl_Position = uLightViewProjection * uModel * vec4(aPosition, 1.0)` — no offset at all.
  implication: Shadow casters are no longer pushed toward the light. At wall-ceiling junctions (grazing angle geometry), the wall top edge doesn't cast shadow onto itself or onto the adjacent ceiling surface deep enough, resulting in a thin lit strip.

- timestamp: 2026-04-05T00:00:00Z
  checked: scene.frag sampleCsmShadow bias values
  found: constantBias = 0.0, normalBias = 0.0 — effectively zero bias in the fragment shader. The system relied entirely on the caster-side offset in the vertex shader.
  implication: Without caster offset AND with zero receiver bias, any grazing geometry is guaranteed to produce self-shadow leaking or missing coverage.

- timestamp: 2026-04-05T00:00:00Z
  checked: SceneRenderPipeline.cpp CSM loop (lines 201-244)
  found: glPolygonOffset(1.1f, 4.0f) is set, but polygon offset alone is insufficient for grazing-angle edges — it scales with the slope of the polygon relative to the light, and at a near-90-degree grazing angle on the wall top edge, the offset is minimal in the direction that matters.
  implication: glPolygonOffset helps with self-shadowing on flat surfaces but doesn't address the coverage gap at geometry junctions.

- timestamp: 2026-04-05T00:00:00Z
  checked: shadow_depth.vert current state
  found: Shader has uModel and uLightViewProjection uniforms only. No uLightDirection, no uShadowCasterOffset.
  implication: Adding uLightDirection (vec3) and uShadowCasterOffset (float) to shadow_depth.vert restores exact pre-regression behavior. The offset is only meaningful for CSM (directional light); spot lights don't need it.

## Resolution

root_cause: Commit b87ee0f removed the geometry shader CSM pipeline (csm_depth.vert + csm_depth.geom) and reused shadow_depth.vert for CSM passes. The old csm_depth.vert applied a 0.18-unit shadow caster offset along the light direction (worldPos += normalize(-uLightDirection) * uShadowCasterOffset) which prevented gaps at grazing-angle junctions. shadow_depth.vert has no such offset. With zero fragment-side bias as well, wall-ceiling junction geometry fails to fully occlude the adjacent surface in depth, producing a bright line artifact.
fix: Add uLightDirection (vec3, default vec3(0)) and uShadowCasterOffset (float, default 0.0) uniforms to shadow_depth.vert. Apply the offset in world space before projecting. In SceneRenderPipeline.cpp CSM loop, set uLightDirection = lighting.sun.direction and uShadowCasterOffset = 0.18f. Spot light passes leave the offset at 0 (or don't set it, relying on default 0.0 from previous frame's clear — actually must set it to 0.0 explicitly for spot passes to avoid bleed-over).
verification: Build passes. Awaiting human confirmation that white line artifact is gone in play preview.
files_changed:
  - assets/shaders/engine/shadow_depth.vert
  - src/engine/rendering/SceneRenderPipeline.cpp
