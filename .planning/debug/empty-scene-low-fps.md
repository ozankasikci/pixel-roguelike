---
status: awaiting_human_verify
trigger: "In the level editor, when pressing Play to enter gameplay preview, the initial scene runs at only ~40 FPS despite being virtually empty."
created: 2026-04-02T00:00:00Z
updated: 2026-04-02T00:45:00Z
---

## Current Focus

hypothesis: Bloom pass running unconditionally + full-res SSAO on Retina display are the primary causes of the low FPS
test: Applied bloom guard and half-res SSAO. Build succeeds. Awaiting human runtime verification.
expecting: FPS improvement from ~40 to closer to 60+ with these two changes
next_action: User verifies FPS in gameplay preview mode

## Symptoms

expected: 60+ FPS on an empty/near-empty scene in gameplay preview mode
actual: ~40 FPS when pressing Play in the editor on the initial scene
errors: None — no crashes or error messages, just poor frame rate
reproduction: Open level editor, load initial scene, press Play button
started: Unknown — user noticed it now, may have been ongoing

## Eliminated

## Evidence

- timestamp: 2026-04-02T00:10:00Z
  checked: SceneRenderPipeline::renderPostProcess()
  found: bloomPass_.render() is called unconditionally on line 298 with no guard for post.enableBloom
  implication: 9 fullscreen passes (5 downsample + 4 upsample) run every frame even when bloom is disabled

- timestamp: 2026-04-02T00:10:01Z
  checked: default.environment
  found: enable_bloom false and ssao_enabled true in the default environment profile
  implication: bloom work is entirely wasted; SSAO adds 2 fullscreen passes with 32 kernel samples

- timestamp: 2026-04-02T00:10:02Z
  checked: initial_scene.scene
  found: ~108 mesh objects, 8 scene lights, plus 4 player torch lights (12 total, 1 shadow-casting spot)
  implication: scene is not truly empty; shadow pass renders all 108 objects unculled for spot shadow + 3 CSM cascades

- timestamp: 2026-04-02T00:10:03Z
  checked: Retina scaling in main.cpp
  found: targetW/targetH use io.DisplayFramebufferScale (2x on Retina), so render resolution is 2x viewport size
  implication: all fullscreen passes (bloom, SSAO, composite, stylize) operate at 4x pixel count vs logical size

## Resolution

root_cause: Multiple rendering inefficiencies compound to drop FPS below 60 on Retina displays. The primary issues are (1) the bloom pass runs 9 fullscreen render passes every frame even when bloom is disabled in the environment profile, and (2) SSAO runs at full Retina resolution (4x pixel count) with 32 kernel samples per pixel. Together with CSM shadow passes (geometry shader on macOS's deprecated OpenGL driver) and ~108 scene objects, these push the frame budget past 16.7ms.
fix: (1) Guard bloomPass_.render() with post.enableBloom check, passing texture ID 0 to composite when bloom is off. (2) Run SSAO at half scene resolution with LINEAR filtering on the blur output for smooth upsampling.
verification: Builds successfully. Awaiting runtime verification.
files_changed:
- src/engine/rendering/SceneRenderPipeline.cpp
- src/engine/rendering/post/SsaoPass.cpp
