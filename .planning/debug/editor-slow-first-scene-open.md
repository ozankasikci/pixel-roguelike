---
status: awaiting_human_verify
trigger: "editor-slow-first-scene-open"
created: 2026-03-29T00:00:00Z
updated: 2026-03-29T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED — MaterialTextureLibrary procedural texture generation (buildBrickSet/buildStoneSet, 512x512 CPU loops) fires lazily on first collectRenderObjects call, not during startup loading screen
test: tracing when ensureTextureSet() is called vs when init() is called
expecting: adding prewarmAllMaterialMaps() to startup loading sequence will absorb the cost behind the loading bar
next_action: await human verification that first scene open is now fast

## Symptoms
<!-- Written during gathering, then IMMUTABLE -->

expected: Scene opens quickly (sub-second)
actual: 2-5 second delay on first scene open in the level editor
errors: None — loads silently
reproduction: Open any scene for the first time after launching the editor. Second and subsequent opens are fast.
started: Unknown

## Eliminated

- hypothesis: Shader compilation on first use
  evidence: Shader constructors compile immediately at object construction time (lines 291-298 of main.cpp, before main loop starts). Not lazy.
  timestamp: 2026-03-29

- hypothesis: Scene switch reinitializes MaterialTextureLibrary (clearing cache)
  evidence: loadSceneIntoEditor() does not call materialTextures.init(). Cache survives scene switches. Second open is fast because cache is warm from first render.
  timestamp: 2026-03-29

## Evidence

- timestamp: 2026-03-29
  checked: MaterialTextureLibrary::init() (MaterialTextureLibrary.cpp:86)
  found: init() only populates resolvedDefinitions_ via resolveMaterialDefinition(). textureSets_ is left empty.
  implication: Texture data is NOT generated during the "Preparing materials" startup step.

- timestamp: 2026-03-29
  checked: MaterialTextureLibrary::resolve() -> ensureTextureSet() -> buildBrickSet()/buildStoneSet()
  found: buildBrickSet iterates 512x512 pixels with 5-8 FBM evaluations per pixel. buildStoneSet same. Both are O(n^2) CPU work. First resolve() call triggers this.
  implication: First collectRenderObjects() call (line 1091 of main.cpp) triggers all procedural material generation for the first time. This is 2-5 seconds of CPU work.

- timestamp: 2026-03-29
  checked: prewarmAllMaterialMaps() in MaterialTextureLibrary.cpp:98
  found: Function exists, iterates all resolved definitions, calls ensureTextureSet() for each material with maps. Never called anywhere (confirmed by grep across entire src/ and apps/).
  implication: prewarmAllMaterialMaps() was written to solve exactly this problem but was never wired in.

- timestamp: 2026-03-29
  checked: startup sequence in apps/level_editor/main.cpp lines 271-352
  found: materialTextures.init(content) is called at line 273, then renderStartupProgress shows "Preparing materials" — but this only resolves definitions, doesn't build texture data. Main loop starts at line 386 with startupViewportHandoffFramesRemaining=3, which shows "Opening workspace..." for 3 frames. On frame 4, previewDirty=true triggers previewWorld.rebuild() then collectRenderObjects() which hits the cold cache.
  implication: The user perceives the frame-4 stall as "first scene open" because that's when the viewport first renders and hangs.

## Resolution

root_cause: MaterialTextureLibrary::prewarmAllMaterialMaps() exists but is never called. Procedural texture generation (buildBrickSet, buildStoneSet — 512x512 CPU loops) fires lazily on the first call to resolve() during the first scene render frame, causing a 2-5 second stall that the user perceives as "first scene open" being slow. The startup loading bar calls materialTextures.init(content) which only resolves definitions; it does not generate texture data. The fix is to call prewarmAllMaterialMaps() during the "Preparing materials" startup step so the cost is absorbed behind the loading screen.
fix: Add materialTextures.prewarmAllMaterialMaps() call in apps/level_editor/main.cpp after materialTextures.init(content), during the startup loading sequence.
verification: Build confirmed clean. Awaiting human confirmation that first scene open is now sub-second.
files_changed: [apps/level_editor/main.cpp]
