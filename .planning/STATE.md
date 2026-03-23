---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: Ready to execute
stopped_at: Completed 01.1-project-restructure-ecs-application-class-modular-engine-game-split/01.1-02-PLAN.md
last_updated: "2026-03-23T18:34:54.048Z"
progress:
  total_phases: 5
  completed_phases: 1
  total_plans: 6
  completed_plans: 4
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-23)

**Core value:** The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable
**Current focus:** Phase 01.1 — project-restructure

## Current Position

Phase: 01.1 (project-restructure) — EXECUTING
Plan: 3 of 4

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

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 01.1 inserted after Phase 1: Project Restructure — ECS, Application class, modular engine/game split (URGENT)

### Blockers/Concerns

- macOS OpenGL cap at 4.1: Research recommended OpenGL 4.6 but macOS only supports 4.1 Core Profile. Phase 1 planning must confirm all required features are available in 4.1 (FBOs, GLSL 4.10, fullscreen quads — all confirmed available).

## Session Continuity

Last session: 2026-03-23T18:34:54.044Z
Stopped at: Completed 01.1-project-restructure-ecs-application-class-modular-engine-game-split/01.1-02-PLAN.md
Resume file: None
