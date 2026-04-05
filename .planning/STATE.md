---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Editor UX
status: Milestone complete
stopped_at: Completed quick/260406-1vc-PLAN.md
last_updated: "2026-04-05T22:30:55.060Z"
last_activity: "2026-04-06 - Completed quick task 260406-0u4: Package only used assets by scanning scene and content references"
progress:
  total_phases: 20
  completed_phases: 16
  total_plans: 56
  completed_plans: 51
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-23)

**Core value:** The Stanley Parable-inspired art style — clean, minimalist environments with warm soft lighting, muted color palette, and stylized realism
**Current focus:** Phase 18 — maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity

## Current Position

Phase: 18
Plan: Not started

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: —
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
| Phase 01-engine-and-dithering-pipeline P01 | 5 | 2 tasks | 17 files |
| Phase 01-engine-and-dithering-pipeline P02 | 15 | 3 tasks | 11 files |
| Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split P01 | 7 | 2 tasks | 20 files |
| Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split P02 | 2 | 2 tasks | 12 files |
| Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split P03 | 3 | 2 tasks | 10 files |
| Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split P04 | 10 | 2 tasks | 3 files |
| Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split P04 | 15 | 3 tasks | 3 files |
| Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output P01 | 90 | 2 tasks | 7 files |
| Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output P02 | 25 | 1 tasks | 5 files |
| Phase 04-improve-lighting-quality P01 | 18 | 2 tasks | 7 files |
| Phase 04-improve-lighting-quality P02 | 13 | 2 tasks | 12 files |
| Phase 04-improve-lighting-quality P03 | 30 | 2 tasks | 15 files |
| Phase 04-improve-lighting-quality P04 | 5 | 2 tasks | 11 files |
| Phase 04-improve-lighting-quality P05 | 9 | 2 tasks | 12 files |
| Phase 05-unify-editor-runtime-build-rendering-parity P01 | 6 | 2 tasks | 6 files |
| Phase 05-unify-editor-runtime-build-rendering-parity P02 | 6 | 2 tasks | 10 files |
| Phase 05-unify-editor-runtime-build-rendering-parity P03 | 30 | 2 tasks | 7 files |
| Phase 06-data-driven-scene-management P01 | 5 | 2 tasks | 11 files |
| Phase 06-data-driven-scene-management P02 | 10 | 2 tasks | 3 files |
| Phase 06-data-driven-scene-management P03 | 15 | 1 tasks | 3 files |
| Phase 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline P01 | 35 | 2 tasks | 19 files |
| Phase 07 P02 | 5 | 3 tasks | 26 files |
| Phase 07 P04 | 525609 | 2 tasks | 6 files |
| Phase 07 P03 | 10 | 2 tasks | 4 files |
| Phase 13-data-driven-behavior-system P01 | 15 | 3 tasks | 10 files |
| Phase 15 P01 | 8 | 2 tasks | 3 files |
| Phase 16 P02 | 8 | 2 tasks | 1 files |
| Phase 17-unified-collider-system-with-trigger-capabilities-and-scripting-support P01 | 180 | 2 tasks | 30 files |
| Phase 17-unified-collider-system-with-trigger-capabilities-and-scripting-support P02 | 30 | 2 tasks | 8 files |
| Phase 17 P04 | 10m | 2 tasks | 4 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P04 | 3 | 1 tasks | 1 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P01 | 25 | 2 tasks | 2 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P03 | 35 | 2 tasks | 25 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P02 | 15 | 2 tasks | 8 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P05 | 20 | 2 tasks | 10 files |
| Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity P06 | 20 | 1 tasks | 8 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Graphics API: OpenGL 4.1 Core Profile (macOS caps at 4.1; research recommended 4.6 but 4.1 is the ceiling on this platform)
- Dithering approach: Post-process fullscreen quad with Bayer matrix; world-space pattern anchoring is non-negotiable from Phase 1
- [Phase 01-engine-and-dithering-pipeline]: GLAD 2 requires LANGUAGES C CXX in CMakeLists.txt project() and jinja2 Python package in venv for code generation
- [Phase 01-engine-and-dithering-pipeline]: uInverseView (pure rotation inverse-view) passed per frame to dither.frag for sphere-map world-space anchoring
- [Phase 01-engine-and-dithering-pipeline]: GL_LINEAR filter on FBO color texture for smooth rendering (changed from GL_NEAREST when dither pass was removed)
- [Phase 01-engine-and-dithering-pipeline]: DitherPass::apply() updated to accept patternScale float parameter so ImGui slider can tune it at runtime
- [Phase 01-engine-and-dithering-pipeline]: Dear ImGui v1.92.6 integrated via CMake FetchContent (GLFW + OpenGL3 backends); ImGui::GetIO().WantCaptureMouse gates camera mouse-look vs UI interaction
- [Phase 01-engine-and-dithering-pipeline]: Quartic attenuation (Pitfall 6 from RESEARCH.md): clamp(1 - pow(dist/radius,4))^2 eliminates hard ring artifacts in Bayer dither output
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: Per-module static library CMake pattern: each lib uses PUBLIC target_include_directories(${CMAKE_SOURCE_DIR}/src) so downstream consumers automatically get include paths
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: EnTT v3.16.0 added via FetchContent as EnTT::EnTT; linked to game lib and executable for plan 02 ECS work
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: Application::run() game loop: init all systems, loop poll/time/update/swap while not closed, shutdown in reverse order
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: MeshComponent includes modelOverride field for CathedralScene arch segments using non-euler rotation
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: CameraComponent initial values match main.cpp hardcoded values (yaw=-90, pitch=0, fov=70, moveSpeed=3.0, near=0.1, far=100)
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: CameraSystem uses identical yaw/pitch/forward math as main.cpp: cos(radians(yaw))*cos(radians(pitch)) for x, sin(radians(pitch)) for y, sin(radians(yaw))*cos(radians(pitch)) for z
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: RenderSystem builds render and light lists from ECS each frame; orchestrates full FBO scene pass + dither post-process + ImGui overlay identical to main.cpp
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: CameraSystem receives InputSystem& by constructor injection -- avoids application-level coupling, makes dependency explicit
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: CathedralScene uses useModelOverride=true for all mesh entities -- geometry is pre-computed with arbitrary transforms that cannot be decomposed to euler angles
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: GLM vec3 scalar constructor is explicit -- must use glm::vec3(0.0f) not brace-init {} in aggregate TransformComponent initialization
- [Phase 01.1-project-restructure-ecs-application-class-modular-engine-game-split]: SceneManager instantiated in main() rather than owned by Application -- keeps Application minimal for this restructure phase
- [Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output]: src/editor/build/ directory ignored by gitignore (build/ pattern) -- files added via git add -f to bypass
- [Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output]: GenericFileScene uses registerCathedralAssets for all scenes -- all current scenes use cathedral asset set
- [Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output]: Build Output panel starts hidden and auto-shows on build start; docked to bottomId alongside Asset Browser
- [Phase 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output]: Cmd+R Build and Run shortcut includes right-mouse-not-pressed guard to avoid conflict with fly camera R (scale tool)
- [Phase 04-improve-lighting-quality]: Explicit if/else chains (not dynamic array indexing) for all shadow sampler accesses — macOS OpenGL driver requirement
- [Phase 04-improve-lighting-quality]: 16-tap Poisson disk PCF with per-fragment hash rotation replaces 9-tap box PCF for banding-free soft shadows
- [Phase 04-improve-lighting-quality]: kMaxShadowedSpotLights expanded from 2 to 8; ShadowRenderData arrays use fill()/loop instead of hardcoded 2-element initializers
- [Phase 04-improve-lighting-quality]: Mip-chain bloom: BloomPass uses 5 GL_RGBA16F half-resolution FBOs, 13-tap downsample + tent upsample, no threshold extraction — HDR values naturally emphasize bright pixels
- [Phase 04-improve-lighting-quality]: Bloom texture bound to unit 8 in CompositePass (units 0-7 pre-occupied by scene/sky/cloud textures); bloomGlow() 8-tap function removed from composite.frag
- [Phase 04-improve-lighting-quality]: Geometry normals written at layout location 2 in scene.frag for SSAO — unperturbed vNormal used, not normal-mapped N, to avoid per-material AO artifacts
- [Phase 04-improve-lighting-quality]: SsaoPass uses fixed RNG seeds (42 kernel, 123 noise) for deterministic results — eliminates temporal SSAO shimmer
- [Phase 04-improve-lighting-quality]: SSAO AO factor applied as color multiply BEFORE bloom and tonemapping in composite.frag — preserves contact shadow darkening
- [Phase 04-improve-lighting-quality]: CascadedShadowMap: GL_TEXTURE_2D_ARRAY with geometry shader invocations=3 for single-pass multi-layer CSM rendering; glFramebufferTexture for all-layer attachment
- [Phase 04-improve-lighting-quality]: LTC tables generated analytically at init() instead of embedding 130KB static arrays — same per-frame cost (texture lookup), avoids binary bloat
- [Phase 04-improve-lighting-quality]: LTC texture units 10/11 sit between shadow map units (8-9) and material map units (12-15) without collision
- [Phase 04-improve-lighting-quality]: emissive_strength=0 default for all materials; non-zero values combine with bloom for perceived light emission
- [Phase 05-unify-editor-runtime-build-rendering-parity]: SceneRenderPipeline in engine_rendering with no game-layer headers — enforces D-06 layering; viewmodelObjects in SceneRenderInput for glDepthRange trick; safeNormalize duplicated as file-local in both pipeline and RSR
- [Phase 05-unify-editor-runtime-build-rendering-parity]: EditorViewportRenderer does NOT own MaterialTextureLibrary -- objects arrive pre-resolved from main.cpp's library, keeping material resolution in one place
- [Phase 05-unify-editor-runtime-build-rendering-parity]: PSSM lambda default 0.5 (balanced linear+log) replaces hardcoded fixed CSM splits (0-5m, 5-20m, 20-far) in CascadedShadowMap
- [Phase 05-unify-editor-runtime-build-rendering-parity]: Model viewer uses SceneRenderPipeline (shadows disabled, no directional lights in viewer setup) per D-10
- [Phase 05-unify-editor-runtime-build-rendering-parity]: SceneRenderPipelineStats uses CPU glfwGetTime() timing (not GPU queries) to avoid sync stalls per D-16
- [Phase 05-unify-editor-runtime-build-rendering-parity]: EditorAssetPreviewRenderer owns its own LtcData instance -- preview renderer is standalone, not routed through SceneRenderPipeline per D-12
- [Phase 06-data-driven-scene-management]: registerAllGameAssets() calls registerDefaults() exactly once — deduplication required since both CathedralAssets and PrisonAssets each called it
- [Phase 06-data-driven-scene-management]: project.cfg stores bare scene filename only (e.g. warden_office.scene), callers prepend assets/scenes/ path — avoids working-directory pitfalls
- [Phase 06-data-driven-scene-management]: runtime main.cpp WardenOfficeScene fallback replaced with GenericFileScene path bridge until Plan 02 adds project.cfg reading
- [Phase 06-data-driven-scene-management]: Pre-loop ImGuiLayer for runtime scene picker is safe — RenderSystem owns its own ImGuiLayer in app.run(), no double-init conflict
- [Phase 06-data-driven-scene-management]: ui.pendingScenePath.empty() is the single empty-state sentinel — all scene-dependent rendering guards on this flag
- [Phase 06-data-driven-scene-management]: doLoadScene lambda defined inline in renderFrame to DRY up loadSceneIntoEditor across Save/Don't Save/direct load paths
- [Phase 06-data-driven-scene-management]: NewScenePopup/DeleteSceneConfirm modals placed outside root window (after assetBrowserActions) matching Save Before Building? pattern
- [Phase 07]: roughness_bias in .material files bakes the per-kind shader default (stone 0.82, wood 0.74, metal 0.34, wax 0.58, moss 0.94, viewmodel 0.48, floor 0.86, brick 0.88)
- [Phase 07]: ContentRegistry::loadMaterialsFromDirectory uses recursive_directory_iterator — any .material file in assets/materials/ or subdirectory is auto-loaded with duplicate detection
- [Phase 07]: MaterialKind enum deleted; feature flags (detailBrick, detailStone, etc.) on ResolvedMaterialDefinition are the authoritative material type source
- [Phase 07]: shading_model key in .material files silently ignored for backward compat — root materials no longer require shading_model
- [Phase 07]: MaterialTextureLibrary.resolve() returns magenta (1,0,1) fallback for unknown materialId — visible in renderer to aid debugging
- [Phase 07]: Roughness formula simplified to clamp(uMaterialRoughnessScale * uMaterialRoughnessBias) — roughness_bias bakes the base roughness value in .material files
- [Phase 07]: pollMaterialHotReload takes MaterialTextureLibrary& by reference — editor owns both and passes them; runtime game never calls this
- [Phase 07]: reloadMaterial takes the materials map so it can immediately re-resolve the updated definition — avoids one-frame magenta flash
- [Phase 07]: test_content_registry links game_rendering (not just game_content) because ContentRegistry.cpp now calls MaterialTextureLibrary methods
- [Phase 07]: Material CRUD popups inline in asset browser panel; file I/O in panel, ContentRegistry mutations delegated to main.cpp via AssetBrowserActionResult
- [Phase 07]: content.addMaterial() replaces content.loadDefaults() for targeted in-memory update on material inspector save
- [Phase 08]: Metal and chained doors from buildScriptedGeometry (not .scene file) so InteractableComponent can be attached
- [Phase 08]: Chain padlock links constructed from cylinder segments; 4 cylinders per link in rectangular loop
- [v1.1 research]: All document mutation APIs complete — `eraseObjects`, `duplicateObject`, `addMesh`, `applyWorldTransform` need no changes
- [v1.1 research]: Selection depth fix is a single-file change in `EditorScenePreviewRenderer.cpp` — remove `ignoreDepth=true` from primary selection overlay pass
- [v1.1 research]: Global shortcuts (Delete, Ctrl+D, Escape, F) must live in `main.cpp` as single handlers with `ImGuiInputFlags_RouteGlobal`; panel-side duplicates removed
- [v1.1 research]: Every new mutation entry point requires explicit capture-before/push-after to `EditorCommandStack` — not enforced by the type system
- [v1.1 research]: `pruneSelection` must be called after every undo/redo call to avoid inspector null-dereference on stale selected IDs
- [v1.1 research]: `duplicateObject()` copies nodeId verbatim — `ensureObjectNodeId()` must be called on duplicate to avoid serialization collision
- [Phase 09]: Two-pass selection overlay: ghost wireframe (ignoreDepth=true, 20% tint) + depth-tested primary wireframe (ignoreDepth=false, full tint)
- [Phase 10]: Camera animation uses ease-out cubic (1-(1-t)^3) for natural deceleration; user input (RMB/MMB/alt+LMB/scroll) cancels in-progress framing animation
- [Phase 10]: Duplicate offset is world-space translation (0.5,0,0) via applyWorldTransform, preserving rotation and scale of duplicated object
- [Phase 10]: Escape guard uses !io.WantTextInput so text field Escape deactivates field first; second Escape clears selectedIds+selectionPicker+inspector context
- [Phase 10]: Hover color (0.55, 0.85, 1.00) produces cool blue-white visually distinct from selection gold without a separate alpha channel
- [Phase 10]: appendHoverOverlay self-guards against selected objects; ignoreDepth=false for depth-tested single pass only (no ghost through geometry)
- [Phase 10]: Selection picker popup overlay removed during verification — hover highlight provides equivalent pre-click affordance without intrusive UI
- [Phase 11]: commitPlacement returns std::optional<uint64_t>: result variable pattern with Mesh case setting it, all others returning nullopt
- [Phase 11]: Add Mesh button placed immediately after Add button with SameLine; filter cleared on each popup open
- [Phase 12]: CameraDebugInfo lives in engine/rendering (camera display data is engine-layer concern, not game-layer)
- [Phase 12]: RuntimeLightingOverride lives in game/rendering (lighting overrides are game-layer concern)
- [Phase 12]: DebugParams retains only UI overlay state plus two embedded sub-structs (CameraDebugInfo camera, RuntimeLightingOverride lighting)
- [Phase 12]: TextureUnits namespace (not enum class) allows direct int use in GL calls without casting
- [Phase 12]: LevelLoadArgs uses pointers for optional levelDef and designated initializer compatibility
- [Phase 12]: GenericFileScene scripted geometry uses static registry pattern — register via registerScriptedGeometry(), not if-chain in onEnter()
- [Phase 12]: File-alias registrations removed from ProceduralGameAssets (pillar, arch, hand, wood_door, etc.) — will be auto-discovered in Plan 04; loadFromFileMulti kept as multi-submesh exception
- [Phase 12]: EventBus [[nodiscard]] subscribe() returns RAII SubscriptionToken; store token in member to keep subscription alive
- [Phase 12]: culledInput pattern: copy SceneRenderInput, swap objects pointer to culledObjects vector — cleanest way to thread culled list through sub-passes without signature changes
- [Phase 12]: FrustumCulling uses Gribb-Hartmann VP matrix extraction; isAabbInsideFrustum transforms 8 local AABB corners to world space before plane test
- [Phase quick]: AI_CONFIG_FBX_CONVERT_TO_M set via SetPropertyBool on both AssimpLoader import paths — converts FBX cm vertices to meters at import, eliminates 0.01 scale workarounds in scene files and scripted geometry
- [Phase 13-data-driven-behavior-system]: NodeIndex built via registry.view<NodeIdComponent>() after all entity placement, not from parallel vectors — ensures scripted geometry entities are also indexed
- [Phase 13-data-driven-behavior-system]: LevelDef.cpp parser refactored to buffer all lines and detect indented sub-lines for behavior/interactable declarations
- [Phase 15]: Theme presets stay repo-local in ImGuiLayer and start from Dear ImGui Dark/Light baselines instead of introducing a skinning framework.
- [Phase 15]: The editor exposes theme comparison only through View -> Interface Theme, matching the existing Interface Font workflow.
- [Phase 16]: End inspector property table early inside switch cases to allow CollapsingHeader behavior sections below for Mesh/Light/Trigger entities
- [Phase 16]: Action categories limited to core subset (Door/Lighting/Audio/Entity/Timing) per D-08 to keep behavior authoring focused
- [Phase 17-unified-collider-system-with-trigger-capabilities-and-scripting-support]: Renamed StaticColliderComponent.h enum to StaticColliderShape to avoid redeclaration conflict with new ColliderShape:uint8_t enum
- [Phase 17-unified-collider-system-with-trigger-capabilities-and-scripting-support]: Editor consolidated BoxCollider/CylinderCollider/Trigger EditorSceneObjectKind to single Collider kind
- [Phase 17-unified-collider-system-with-trigger-capabilities-and-scripting-support]: Parser reads old collider_box/collider_cylinder/trigger_box/trigger_sphere and new collider <shape> <mode> format; serializer emits only new format
- [Phase 17]: CharacterVirtual sensor detection requires CharacterContactListener (not global ContactListener) — registered via SetListener on CharacterVirtual
- [Phase 17]: SolidAndTrigger mode creates two separate Jolt bodies (solid + sensor) sharing same shape/position
- [Phase 17]: TriggerSystem deleted — sensor overlap detection now done by PhysicsSystem via Jolt mIsSensor bodies
- [Phase 17]: Yellow tint (0.85,0.75,0) unselected / (1.0,0.95,0.30) selected for SolidAndTrigger wireframes across all three editor rendering sites
- [Phase 18]: Use sizeof(T)==0 static_assert (not static_assert(false)) for C++20 compatible exhaustiveness in variant visitors
- [Phase 18]: std::visit with if constexpr chains replaces switch-on-kind in EditorSceneDocument for compile-guided extensibility
- [Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity]: PlacementBase defined as documentation type in LevelDef.h but not embedded as named field in existing placement structs — avoids 50+ callsite churn while fulfilling acceptance criteria
- [Phase 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity]: parseNodeMetadata uses out-parameter references to enable clean 'if (...) { continue; }' caller pattern in all placement parser loops
- [Phase 18]: Inspector decomposition: per-type inspector free functions in src/editor/ui/inspectors/, each closes the property table before post-table rendering
- [Phase 18]: AssetInspectorSession extracted to shared header enabling Material/Environment/Prefab inspector file separation
- [Phase 18]: Unified light format: 'light point/spot/directional' replaces separate light/spot_light/dir_light keywords
- [Phase 18]: Shadow bool for spot lights remains positional read before collectRemainingTokens to preserve field order
- [Phase 18]: drawPositionSection added to InspectorUtils for inspectors that only edit position (Light, Archetype, ReflectionProbe, PlayerSpawn)
- [Phase 18]: EditorInspectorPanel.cpp reduced from 422 to 81 lines: extracted AssetInspectorHelpers, AssetInspectorSession, and SceneSelectionInspector into dedicated files

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 01.1 inserted after Phase 1: Project Restructure — ECS, Application class, modular engine/game split (URGENT)
- Phase 02.1 inserted after Phase 2: Equipment Inventory and Carry Weight — weapons are inventory items, no slot-grid inventory, Dark Souls-style carry weight (URGENT)
- Phase 3 added: Build menu in editor — macOS build from editor with progress and output (Windows support later)
- Phase 4 added: Improve lighting quality — research best practices and implement industry-standard real-time lighting
- Phase 5 added: Unify editor/runtime/build rendering parity — shared render path so all three modes produce identical visual output
- Phase 6 added: Data-driven scene management — New/Delete Scene in editor, configurable runtime default, remove legacy hardcoded scene classes
- Phase 7 added: Data-driven material system — replace hardcoded materials with a proper material pipeline
- Phase 15 added: prototype editor ImGui themes and choose a final skin direction
- Phase 16 added: Editor trigger and behavior authoring with save-load fidelity
- Phase 17 added: Unified collider system with trigger capabilities and scripting support
- Phase 18 added: Maintainability refactoring for editor UI, serialization, and round-trip fidelity

### Blockers/Concerns

- macOS OpenGL cap at 4.1: Research recommended OpenGL 4.6 but macOS only supports 4.1 Core Profile. Phase 1 planning must confirm all required features are available in 4.1 (FBOs, GLSL 4.10, fullscreen quads — all confirmed available).

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260329-t50 | Fix lighting attenuation and intensity | 2026-03-29 | 459e34f | [260329-t50-fix-lighting-attenuation-and-intensity](./quick/260329-t50-fix-lighting-attenuation-and-intensity/) |
| 260329-uom | Raise warden office ceiling from 2.5m to 3.5m | 2026-03-29 | 97c3880 | [260329-uom-fix-warden-office-room-being-too-small-i](./quick/260329-uom-fix-warden-office-room-being-too-small-i/) |
| 260329-uy6 | Match Stanley Parable lighting and color palette | 2026-03-29 | 0a1f494 | [260329-uy6-match-stanley-parable-lighting-and-color](./quick/260329-uy6-match-stanley-parable-lighting-and-color/) |
| 260329-x0q | Fix pixelated rendering and clean up old dither artifacts | 2026-03-29 | a5fa67a | [260329-x0q-fix-pixelated-low-resolution-rendering-i](./quick/260329-x0q-fix-pixelated-low-resolution-rendering-i/) |
| 260330-0fz | Implement disk-based asset cache for meshes and procedural textures | 2026-03-30 | a2b1c1e | [260330-0fz-implement-disk-based-asset-cache-for-mes](./quick/260330-0fz-implement-disk-based-asset-cache-for-mes/) |
| 260330-171 | Comprehensive AssetCache test suite for invalidation, binary format, and edge cases | 2026-03-30 | 0ed7241 | [260330-171-comprehensive-assetcache-test-suite-for-](./quick/260330-171-comprehensive-assetcache-test-suite-for-/) |
| 260330-222 | Port AudioSystem, ActionMap/InputSystem, GameOverlays, EditorConsoleSink from codex/scripting-v1 | 2026-03-30 | ecc270a | [260330-222-port-audiosystem-actionmap-inputsystem-g](./quick/260330-222-port-audiosystem-actionmap-inputsystem-g/) |
| 260330-321 | Add concrete wall texture material to warden office | 2026-03-30 | 3187a34 | [260330-321-add-concrete-wall-texture-material-to-wa](./quick/260330-321-add-concrete-wall-texture-material-to-wa/) |
| 260330-mzu | Restore post-processing flags disabled by SSAO commit | 2026-03-30 | 19068c7 | [260330-mzu-fix-editor-scene-objects-not-visible-aft](./quick/260330-mzu-fix-editor-scene-objects-not-visible-aft/) |
| 260330-rwe | Create a Claude Code skill for procedural texture generation | 2026-03-30 | e976302 | [260330-rwe-create-a-claude-code-skill-for-procedura](./quick/260330-rwe-create-a-claude-code-skill-for-procedura/) |
| 260330-wc2 | Fix too-fast scrollbar scrolling in the editor asset browser panel | 2026-03-30 | 156472e | [260330-wc2-fix-too-fast-scrollbar-scrolling-in-the-](./quick/260330-wc2-fix-too-fast-scrollbar-scrolling-in-the-/) |
| 260330-wt0 | Reorganize Environment inspector panel into logical sub-groups | 2026-03-30 | 1824913 | [260330-wt0-improve-environment-inspector-panel-ux-b](./quick/260330-wt0-improve-environment-inspector-panel-ux-b/) |
| 260401-qau | Fix delete key on macOS: add ImGuiKey_Backspace as alternative trigger | 2026-04-01 | 698ccf3 | [260401-qau-fix-delete-key-on-macos-add-imguikey-bac](./quick/260401-qau-fix-delete-key-on-macos-add-imguikey-bac/) |
| 260401-rt2 | Cache shader uniform locations, guard Jolt body ID, deduplicate MathUtils, reuse renderer vectors | 2026-04-01 | 639ae43 | [260401-rt2-codebase-cleanup-cache-uniform-locations](./quick/260401-rt2-codebase-cleanup-cache-uniform-locations/) |
| 260401-uie | Selection undo/redo: Ctrl+Z restores selection changes in level editor | 2026-04-01 | b871d75 | [260401-uie-make-ctrl-z-also-work-for-selection-dese](./quick/260401-uie-make-ctrl-z-also-work-for-selection-dese/) |
| 260401-uvc | Fix FBX mesh import scaling: AI_CONFIG_FBX_CONVERT_TO_M eliminates 0.01 workarounds | 2026-04-01 | 5d0b37e | [260401-uvc-investigate-mesh-import-scaling-research](./quick/260401-uvc-investigate-mesh-import-scaling-research/) |
| 260401-x6s | Fix editor high CPU usage with industry-standard idle throttling | 2026-04-01 | 7e0c66f | [260401-x6s-fix-editor-high-cpu-usage-with-industry-](./quick/260401-x6s-fix-editor-high-cpu-usage-with-industry-/) |
| 260402-0nx | Extend mesh asset discovery to scan assets/packs/ in addition to assets/meshes/ | 2026-04-02 | 24dcba0 | [260402-0nx-extend-mesh-asset-discovery-to-scan-asse](./quick/260402-0nx-extend-mesh-asset-discovery-to-scan-asse/) |
| 260402-0wi | Fix inspector transform not updating mesh: add EditorPreviewWorld::syncTransforms() | 2026-04-02 | 818a2c4 | [260402-0wi-fix-inspector-transform-not-updating-mes](./quick/260402-0wi-fix-inspector-transform-not-updating-mes/) |
| 260402-1l5 | Fix QuestDoorsPack door scale: measured FBX AABB (~9.2m height), corrected scene scale from 0.25 to 0.21 | 2026-04-02 | 5d97e82 | [260402-1l5-investigate-and-fix-questdoorspack-fbx-d](./quick/260402-1l5-investigate-and-fix-questdoorspack-fbx-d/) |
| 260402-2lb | Fix multi-object gizmo scale compounding: MultiGizmoState caches original transforms per drag | 2026-04-02 | b9a2d31 | [260402-2lb-fix-multi-object-scaling-gizmo-losing-pr](./quick/260402-2lb-fix-multi-object-scaling-gizmo-losing-pr/) |
| 260402-3at | Add EditorSceneObjectKind::Group with LevelGroupNode for hierarchy grouping | 2026-04-02 | ed27e6e | [260402-3at-add-editorsceneobjectkind-group-with-lev](./quick/260402-3at-add-editorsceneobjectkind-group-with-lev/) |
| 260402-3o3 | Add "Create Group" to outliner right-click context menu with centroid placement and undo/redo | 2026-04-02 | 5c5f71c | [260402-3o3-add-create-group-to-outliner-right-click](./quick/260402-3o3-add-create-group-to-outliner-right-click/) |
| 260402-3tl | Add Ctrl/Cmd+S keyboard shortcut to save scene in level editor | 2026-04-02 | 5cd4b53 | [260402-3tl-pressing-ctrl-or-cmd-plus-s-should-save-](./quick/260402-3tl-pressing-ctrl-or-cmd-plus-s-should-save-/) |
| 260402-3qe | Implement Unity-style double-F focus: two zoom levels on repeated F key press | 2026-04-02 | 284f77f | [260402-3qe-implement-unity-style-double-f-focus-two](./quick/260402-3qe-implement-unity-style-double-f-focus-two/) |
| 260402-41r | Auto-scroll outliner to selected object on viewport selection, with ancestor group auto-expand | 2026-04-02 | dee2921 | [260402-41r-when-i-select-an-object-in-the-scene-it-](./quick/260402-41r-when-i-select-an-object-in-the-scene-it-/) |
| 260402-4gx | Fix scale gizmo uniform drag to use Unity-style screen-space radial direction | 2026-04-02 | 8b1e428 | [260402-4gx-fix-scale-gizmo-drag-direction-to-work-c](./quick/260402-4gx-fix-scale-gizmo-drag-direction-to-work-c/) |
| 260402-4is | Make player torch light adjustable in debug overlay | 2026-04-02 | c7de887 | [260402-4is-investigate-player-attached-light-and-ma](./quick/260402-4is-investigate-player-attached-light-and-ma/) |
| 260402-d7q | Wire player torch override into level editor Environment panel with full persistence | 2026-04-02 | e51d45d | [260402-d7q-wire-player-torch-override-into-level-ed](./quick/260402-d7q-wire-player-torch-override-into-level-ed/) |
| 260402-dec | Shadow frustum culling and render state sorting for performance | 2026-04-02 | 1a15376 | [260402-dec-shadow-frustum-culling-and-render-state-](./quick/260402-dec-shadow-frustum-culling-and-render-state-/) |
| 260402-n8o | Add search filter bar to outliner scene hierarchy panel | 2026-04-02 | 57ce06c | [260402-n8o-add-search-filter-bar-to-outliner-scene-](./quick/260402-n8o-add-search-filter-bar-to-outliner-scene-/) |
| 260402-okw | Implement editor debug harness with Unix socket remote control | 2026-04-02 | f807b20 | [260402-okw-implement-editor-debug-harness-with-unix](./quick/260402-okw-implement-editor-debug-harness-with-unix/) |
| 260402-prr | Write tests for the editor debug harness | 2026-04-02 | 28e297f | [260402-prr-write-tests-for-the-editor-debug-harness](./quick/260402-prr-write-tests-for-the-editor-debug-harness/) |
| 260402-q3f | Debug harness input simulation (keyPress/mouseClick/drag) and gizmo diagnostic script | 2026-04-02 | 08b8502 | [260402-q3f-use-debug-harness-to-diagnose-why-gizmo-](./quick/260402-q3f-use-debug-harness-to-diagnose-why-gizmo-/) |
| 260402-rkd | Expand debug harness with entity listing, focus camera, gizmo-aware drag, wait-for-idle | 2026-04-02 | edbfff1 | [260402-rkd-expand-debug-harness-with-entity-listing](./quick/260402-rkd-expand-debug-harness-with-entity-listing/) |
| 260402-tpl | Document the EditorCommander debugging harness for future debugging use | 2026-04-02 | — | [260402-tpl-document-the-editorcommander-debugging-h](./quick/260402-tpl-document-the-editorcommander-debugging-h/) |
| 260402-uc1 | Use QuestDoorsPack walls/floor in initial scene, fix door gap/size, remove trim | 2026-04-02 | 86fa32e | [260402-uc1-use-questdoorspack-walls-and-floor-in-in](./quick/260402-uc1-use-questdoorspack-walls-and-floor-in-in/) |
| 260402-uz2 | Fix walls/floors to use QuestDoorsPack textured materials with mesh UVs | 2026-04-02 | f7d1abe | [260402-uz2-fix-walls-and-floors-to-use-questdoorspa](./quick/260402-uz2-fix-walls-and-floors-to-use-questdoorspa/) |
| 260403-udy | Fix play preview texture corruption after moving a scene mesh with the gizmo | 2026-04-03 | 9d1ee2b | [260403-udy-fix-play-preview-texture-corruption-afte](./quick/260403-udy-fix-play-preview-texture-corruption-afte/) |
| 260403-v4x | Fix editor freezing after gizmo or inspector scene changes trigger runtime preview rebuild | 2026-04-03 | ff7ce59 | [260403-v4x-fix-editor-freezing-after-gizmo-or-inspe](./quick/260403-v4x-fix-editor-freezing-after-gizmo-or-inspe/) |
| 260403-wsw | Enable arrow-key navigation in the hierarchy panel | 2026-04-03 | 534ed10 | [260403-wsw-enable-arrow-key-navigation-in-the-hiera](./quick/260403-wsw-enable-arrow-key-navigation-in-the-hiera/) |
| 260404-omb | Fix editor light gizmo moving helper but not preview light | 2026-04-04 | 1464a2a | [260404-omb-fix-editor-light-gizmo-moving-helper-but](./quick/260404-omb-fix-editor-light-gizmo-moving-helper-but/) |
| 260404-p8q | Safe cleanup of environment shader properties: remove redundant sky sun fields, unify sky enable toggle, stop persisting depth preview scale | 2026-04-04 | 5bad39e | [260404-p8q-safe-cleanup-of-environment-shader-prope](./quick/260404-p8q-safe-cleanup-of-environment-shader-prope/) |
| 260405-4rg | Reduce editVec3 default drag speed from 0.1 to 0.01 for finer precision | 2026-04-05 | 9fcaff1 | [260405-4rg-just-like-unity-for-float-and-int-values](./quick/260405-4rg-just-like-unity-for-float-and-int-values/) |
| 260405-587 | Enable single-click text input on DragFloat inspector fields | 2026-04-05 | 6351cfe | [260405-587-single-click-on-inspector-fields-should-](./quick/260405-587-single-click-on-inspector-fields-should-/) |
| 260405-5em | Update ROADMAP.md to mark phases 4, 13-17 as complete | 2026-04-05 | be31435 | [260405-5em-update-roadmap-md-to-mark-completed-phas](./quick/260405-5em-update-roadmap-md-to-mark-completed-phas/) |
| 260405-hxx | Implement editor-only performance metrics system to track system execution times | 2026-04-05 | 0af2746 | [260405-hxx-implement-editor-only-performance-metric](./quick/260405-hxx-implement-editor-only-performance-metric/) |
| 260405-ify | Remove CSM double-draw hack and replace with GPU polygon offset bias | 2026-04-05 | 01e191a | [260405-ify-remove-csm-double-draw-hack-and-replace-](./quick/260405-ify-remove-csm-double-draw-hack-and-replace-/) |
| 260405-isu | Replace geometry shader CSM with multi-pass rendering + front-face culling | 2026-04-05 | b87ee0f | [260405-isu-replace-geometry-shader-csm-with-multi-p](./quick/260405-isu-replace-geometry-shader-csm-with-multi-p/) |
| 260405-nb9 | Auto-detect project root from executable path so game works when double-clicked | 2026-04-05 | 2c102cb | [260405-nb9-auto-detect-project-root-from-executable](./quick/260405-nb9-auto-detect-project-root-from-executable/) |
| 260405-wd7 | Fix crash on missing assets and ensure CI packages all required files | 2026-04-05 | 8d3a4d1 | [260405-wd7-fix-crash-on-missing-assets-and-ensure-c](./quick/260405-wd7-fix-crash-on-missing-assets-and-ensure-c/) |
| 260405-x13 | Fix asset discovery so game works when distributed outside project tree | 2026-04-05 | 8709a66 | [260405-x13-fix-asset-discovery-so-game-works-when-d](./quick/260405-x13-fix-asset-discovery-so-game-works-when-d/) |
| 260406-04r | Add Package for Sharing button to editor Build menu | 2026-04-06 | c796970 | [260406-04r-add-build-package-button-to-editor-that-](./quick/260406-04r-add-build-package-button-to-editor-that-/) |
| 260406-0u4 | Package only used assets by scanning scene and content references | 2026-04-06 | 5ad8797 | [260406-0u4-package-only-used-assets-by-scanning-sce](./quick/260406-0u4-package-only-used-assets-by-scanning-sce/) |

## Session Continuity

Last activity: 2026-04-06 - Completed quick task 260406-0u4: Package only used assets by scanning scene and content references
Last session: 2026-04-05T22:30:55.049Z
Stopped at: Completed quick/260406-1vc-PLAN.md
Resume file: None
