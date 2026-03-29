---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: Ready to execute
stopped_at: Completed 04-03-PLAN.md
last_updated: "2026-03-29T17:41:28.243Z"
progress:
  total_phases: 6
  completed_phases: 5
  total_plans: 13
  completed_plans: 13
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-23)

**Core value:** The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable
**Current focus:** Phase 04 — make-engine-fully-generic-clean-engine-game-boundary-remove-hardcoded-game-content-generic-typescript-scripting-generic-editor-and-build-system

## Current Position

Phase: 04 (make-engine-fully-generic-clean-engine-game-boundary-remove-hardcoded-game-content-generic-typescript-scripting-generic-editor-and-build-system) — EXECUTING
Plan: 3 of 3

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
| Phase 04-make-engine-fully-generic P01 | 25 | 2 tasks | 13 files |
| Phase 04-make-engine-fully-generic P03 | 524 | 2 tasks | 11 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Graphics API: OpenGL 4.1 Core Profile (macOS caps at 4.1; research recommended 4.6 but 4.1 is the ceiling on this platform)
- Dithering approach: Post-process fullscreen quad with Bayer matrix; world-space pattern anchoring is non-negotiable from Phase 1
- [Phase 01-engine-and-dithering-pipeline]: GLAD 2 requires LANGUAGES C CXX in CMakeLists.txt project() and jinja2 Python package in venv for code generation
- [Phase 01-engine-and-dithering-pipeline]: uInverseView (pure rotation inverse-view) passed per frame to dither.frag for sphere-map world-space anchoring
- [Phase 01-engine-and-dithering-pipeline]: GL_NEAREST filter on FBO color texture critical to prevent interpolation in dither pass input
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
- [Phase 04-make-engine-fully-generic]: Engine layer uses int shadingModelIndex=0 as default (0=Stone), game layer casts MaterialKind to int at the engine/game boundary
- [Phase 04-make-engine-fully-generic]: MaterialTextureLibrary.resolve() takes only materialId string; legacyKind parameter removed; fallback goes directly to stone_default
- [Phase 04-make-engine-fully-generic]: MeshComponent.material (MaterialKind) removed; materialId string is the sole material identifier in ECS components
- [Phase 04-make-engine-fully-generic]: PlayerTorchComponent uses vector<RenderLight> computedLights field written by updatePlayerTorch each frame; collectLights just iterates this vector
- [Phase 04-make-engine-fully-generic]: Engine overlay boundary: ImGuiLayer only manages lifecycle and engine debug overlay; all game HUD/overlay code lives in game/ui/GameOverlays namespace

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 01.1 inserted after Phase 1: Project Restructure — ECS, Application class, modular engine/game split (URGENT)
- Phase 02.1 inserted after Phase 2: Equipment Inventory and Carry Weight — weapons are inventory items, no slot-grid inventory, Dark Souls-style carry weight (URGENT)
- Phase 3 added: Build menu in editor — macOS build from editor with progress and output (Windows support later)
- Phase 4 added: Make engine fully generic — clean engine/game boundary, remove hardcoded game content, generic TypeScript scripting, generic editor and build system

### Blockers/Concerns

- macOS OpenGL cap at 4.1: Research recommended OpenGL 4.6 but macOS only supports 4.1 Core Profile. Phase 1 planning must confirm all required features are available in 4.1 (FBOs, GLSL 4.10, fullscreen quads — all confirmed available).

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260329-6wt | Add a proper inspector panel section for scripts | 2026-03-29 | 45785e3 | [260329-6wt-add-a-proper-inspector-panel-section-for](./quick/260329-6wt-add-a-proper-inspector-panel-section-for/) |
| 260329-imv | Implement editor Console panel with spdlog ring-buffer sink | 2026-03-29 | 09c34ae | [260329-imv-implement-editor-console-panel-with-scri](./quick/260329-imv-implement-editor-console-panel-with-scri/) |

## Session Continuity

Last session: 2026-03-29T17:41:28.240Z
Stopped at: Completed 04-03-PLAN.md
Resume file: None
