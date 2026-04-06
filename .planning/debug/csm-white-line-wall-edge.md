---
status: awaiting_human_verify
trigger: "Wall-edge-white-line-regression: bright line at wall-ceiling and wall-floor junctions has regressed again. Previous debug session fixed it but the fix keeps being lost."
created: 2026-04-05T00:00:00Z
updated: 2026-04-06T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED. The regression cycle is architectural: caster-side offset fixes wall-ceiling but creates floor artifacts; receiver-side normal offset fixes floor but does NOT fix the wall surface at the ceiling junction. The two approaches are not symmetric — they fix different surfaces. A combined approach is required.

test: Apply small caster offset (0.06) + keep receiver-side normal offset (0.15). The caster offset closes the wall-ceiling gap; the receiver-side offset closes the floor gap; 0.06 is too small to create visible floor triangle artifacts.

expecting: Both wall-ceiling and wall-floor junctions are clean; no white line artifact, no triangle artifacts.

next_action: Implement combined fix: restore caster offset to 0.06f in SceneRenderPipeline.cpp + document the full dual-mechanism in both shader and C++ with cross-references

## Symptoms

expected: Clean shadow transitions at wall-ceiling and wall-floor junctions — no bright line artifacts
actual: A bright/white line appears along edges where walls meet ceilings/floors. This is a rendering artifact visible in play preview mode.
errors: No errors — purely a visual artifact
reproduction: Launch level-editor, open initial_scene, enter play preview (Cmd+P), look at edges where walls meet ceiling or floor. White/bright line visible.
started: Has regressed multiple times. Originally after commit b87ee0f (replaced geometry shader CSM with multi-pass rendering). Was fixed, but keeps coming back.

## Eliminated

- hypothesis: Front-face culling is causing the artifact
  evidence: User already tried removing front-face culling — it did not help.
  timestamp: 2026-04-05T00:00:00Z

- hypothesis: Zero-value normalBias in scene.frag is the problem (the `0.0 * receiverSlope` depth-space bias)
  evidence: This is a depth-space bias for self-shadowing, unrelated to the coverage gap artifact. The coverage gap is fixed only by positional offsets (caster or receiver world-space).
  timestamp: 2026-04-06T00:00:00Z

- hypothesis: Receiver-side normal offset alone (N * 0.15 * slopeFactor) is sufficient to fix both wall-ceiling and wall-floor junction lines
  evidence: The receiver-side offset pushes the shadow lookup along the fragment's surface normal. For a WALL fragment at the ceiling junction (N=horizontal), this moves the sample horizontally — NOT in the direction that closes the ceiling coverage gap. The gap is between the wall's top edge and the ceiling; pushing the wall sample point sideways doesn't cover this. The receiver-side approach only works for surfaces where N has a vertical component (floor/ceiling fragments at junctions). This is why the wall-ceiling line keeps regressing when the caster offset is removed.
  timestamp: 2026-04-06T00:00:00Z

- hypothesis: Caster offset 0.18 alone is sufficient
  evidence: 0.18 units is large enough to create triangle artifacts at wall-floor junctions from first-person view. These appear as tessellation-shaped bright triangles at the base of walls. Eliminated at commit ade6828 → replaced by receiver-side approach.
  timestamp: 2026-04-06T00:00:00Z

## Evidence

- timestamp: 2026-04-05T00:00:00Z
  checked: git diff of commit b87ee0f (the introducing commit)
  found: The old csm_depth.vert had `worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;` with uShadowCasterOffset = 0.18f set from C++. The new shadow_depth.vert is `gl_Position = uLightViewProjection * uModel * vec4(aPosition, 1.0)` — no offset at all.
  implication: Shadow casters are no longer pushed toward the light. At wall-ceiling junctions (grazing angle geometry), the wall top edge doesn't cast shadow far enough, resulting in a thin lit strip.

- timestamp: 2026-04-05T00:00:00Z
  checked: scene.frag sampleCsmShadow bias values
  found: constantBias = 0.0, normalBias = 0.0 * receiverSlope — effectively zero depth-space bias. The depth-space bias doesn't address coverage gaps.
  implication: Coverage gaps must be fixed by world-space positional offsets (caster side or receiver side), not depth-space bias.

- timestamp: 2026-04-05T00:00:00Z
  checked: SceneRenderPipeline.cpp CSM loop glPolygonOffset usage
  found: glPolygonOffset(1.1f, 4.0f) is set for CSM passes.
  implication: glPolygonOffset scales with polygon slope relative to the light. At near-90-degree grazing angles (wall top edge facing the sun nearly edge-on), the depth offset is minimal. Insufficient to close coverage gaps at junctions.

- timestamp: 2026-04-06T00:00:00Z
  checked: Commit history for shadow_depth.vert and SceneRenderPipeline.cpp
  found: Regression cycle: b0a35fa added caster offset 0.18 (fixed wall-ceiling). Then quick task 260406-29h added receiver-side normal offset to scene.frag (commit 863b242) and zeroed the caster offset (commit ade6828, 12 minutes before this investigation), claiming receiver-side handles both junction types.
  implication: The claim that receiver-side handles wall-ceiling junctions is incorrect. Receiver-side with N=horizontal (wall surface) shifts the sample sideways, not in the coverage-gap direction.

- timestamp: 2026-04-06T00:00:00Z
  checked: scene.frag sampleCsmShadow receiver-side offset implementation
  found: `vec3 biasedWorldPos = fragWorldPos + N * 0.15 * csmSlopeFactor` — pushes the lookup position along the surface normal before CSM projection. For floor/ceiling receivers (N vertical), this moves the lookup deeper into the shadow correctly. For wall receivers (N horizontal), this moves the lookup sideways — does NOT close the ceiling coverage gap.
  implication: Receiver-side offset is correct for floor/ceiling surfaces but insufficient for wall surfaces. Wall-ceiling white line WILL reappear with only receiver-side offset active.

- timestamp: 2026-04-06T00:00:00Z
  checked: Why 0.18 caused floor artifacts
  found: The 0.18 caster offset pushes ALL shadow geometry 0.18 units toward the sun (upward for a sun from above). This means the wall's shadow map coverage starts 0.18 world units above the actual wall base — creating a gap where the floor meets the wall. The gap was visible as tessellation-shaped triangles from first-person view.
  implication: A smaller caster offset (e.g. 0.06) would still close the wall-ceiling gap (the gap at the top edge is much smaller than the offset needed to close it) while the floor gap of 0.06 units would be sub-pixel and covered by the existing receiver-side offset.

## Resolution

root_cause: The regression cycle is caused by oscillating between two incomplete solutions:
  - Caster offset 0.18: closes wall-ceiling gap (casters pushed toward light, shadow map coverage extended to wall top edge) but creates floor artifacts (wall shadow starts 0.18 units above floor)
  - Receiver-side normal offset 0.15 alone: closes floor gap (pushes lookup along vertical N deeper into shadow) but does NOT close wall-ceiling gap (horizontal N pushes lookup sideways, not in the gap direction)
  Neither approach alone handles both junction types. The fix requires BOTH: a small caster offset (0.06) for wall-ceiling gap coverage, PLUS the receiver-side normal offset for floor gap coverage.

fix: Restored caster offset to 0.06f (was 0.0f after commit ade6828, was 0.18f originally) in SceneRenderPipeline.cpp CSM loop. Kept receiver-side normal offset 0.15 in scene.frag. Added detailed dual-mechanism documentation with cross-references in all three files (SceneRenderPipeline.cpp, shadow_depth.vert, scene.frag) explaining why BOTH mechanisms are required and what each one fixes, so future refactors cannot remove either without understanding the consequences.

verification: Build compiles successfully (level-editor target, 100%). Awaiting user confirmation that both wall-ceiling white line and wall-floor triangle artifacts are resolved.

files_changed:
  - src/engine/rendering/SceneRenderPipeline.cpp
  - assets/shaders/engine/shadow_depth.vert
  - assets/shaders/game/scene.frag
