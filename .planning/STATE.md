---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: Milestone complete
stopped_at: Phase 6 context gathered
last_updated: "2026-03-30T18:04:07.934Z"
last_activity: "2026-03-30 - Completed quick task 260330-rwe: Create a Claude Code skill for procedural texture generation"
progress:
  total_phases: 8
  completed_phases: 6
  total_plans: 18
  completed_plans: 18
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-23)

**Core value:** The Stanley Parable-inspired art style — clean, minimalist environments with warm soft lighting, muted color palette, and stylized realism
**Current focus:** Phase 05 — unify-editor-runtime-build-rendering-parity

## Current Position

Phase: 05
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

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 01.1 inserted after Phase 1: Project Restructure — ECS, Application class, modular engine/game split (URGENT)
- Phase 02.1 inserted after Phase 2: Equipment Inventory and Carry Weight — weapons are inventory items, no slot-grid inventory, Dark Souls-style carry weight (URGENT)
- Phase 3 added: Build menu in editor — macOS build from editor with progress and output (Windows support later)
- Phase 4 added: Improve lighting quality — research best practices and implement industry-standard real-time lighting
- Phase 5 added: Unify editor/runtime/build rendering parity — shared render path so all three modes produce identical visual output
- Phase 6 added: Data-driven scene management — New/Delete Scene in editor, configurable runtime default, remove legacy hardcoded scene classes

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

## Session Continuity

Last activity: 2026-03-30 - Completed quick task 260330-rwe: Create a Claude Code skill for procedural texture generation
Last session: 2026-03-30T18:04:07.929Z
Stopped at: Phase 6 context gathered
Resume file: .planning/phases/06-data-driven-scene-management/06-CONTEXT.md
