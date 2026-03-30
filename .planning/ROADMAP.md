# Roadmap: 3D Roguelike

## Overview

Build a first-person 3D psychological horror game with a custom C++ engine, starting with the rendering pipeline and working bottom-up through player movement, combat, enemies, and finally the game systems that make it shippable. Every phase delivers something runnable. The Stanley Parable-inspired visual style (clean surfaces, warm lighting, muted palette) is the foundation that all gameplay builds on.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Engine and Rendering Pipeline** - Custom C++ engine with OpenGL 4.1, stylize post-process with edge detection and tone mapping
- [ ] **Phase 1.1: Project Restructure** - ECS, Application class, modular engine/game split (INSERTED)
- [ ] **Phase 2: Player, Environment, and Lighting** - Player moves through a lit prison environment with collision; warm lighting and material system

## Phase Details

### Phase 1: Engine and Rendering Pipeline
**Goal**: The rendering pipeline produces clean, stylized 3D output — geometry renders through the post-process pipeline with edge detection, tone mapping, and smooth lighting
**Depends on**: Nothing (first phase)
**Requirements**: RNDR-01, RNDR-02, RNDR-03, RNDR-04, RNDR-05
**Success Criteria** (what must be TRUE):
  1. A 3D scene renders to screen through the stylize post-process with edge detection and tone mapping
  2. Camera movement is smooth with no visual artifacts
  3. The scene renders at display resolution with GL_LINEAR filtering for smooth output
  4. Point light sources produce warm, soft illumination with correct attenuation
  5. A Dear ImGui debug overlay lets the developer tune post-process parameters at runtime without recompiling
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md — CMake build system, OpenGL 4.1 engine bootstrap, FBO + dither post-process with world-space anchoring
- [x] 01-02-PLAN.md — Blinn-Phong point lighting, cathedral test scene, ImGui debug overlay

### Phase 01.1: Project Restructure — ECS, Application class, modular engine/game split (INSERTED)

**Goal:** Transform the flat project structure into a modular engine/game architecture with ECS (EnTT), Application class owning the game loop, event bus, and SceneManager — all existing rendering functionality continues working identically
**Requirements**: None (structural refactoring, not mapped to feature requirements)
**Depends on:** Phase 1
**Success Criteria** (what must be TRUE):
  1. main.cpp is under 50 lines — creates Application, registers systems, pushes scene, calls run()
  2. All game objects (meshes, lights, camera) are ECS entities with components, processed by systems
  3. CMake builds per-module static libraries (engine_core, engine_rendering, engine_ui, engine_input, engine_scene, game)
  4. Visual output is identical to pre-restructure: dithering, edge detection, fog, ImGui overlay, screenshots, camera all work
**Plans**: 4 plans

Plans:
- [x] 01.1-01-PLAN.md — Directory restructure, CMake modular build, EnTT FetchContent
- [x] 01.1-02-PLAN.md — Engine core (Application, System, EventBus, Time) + ECS components
- [x] 01.1-03-PLAN.md — SceneManager, InputSystem, CameraSystem, RenderSystem
- [x] 01.1-04-PLAN.md — CathedralScene ECS refactor, new main.cpp, visual verification

### Phase 2: Player, Environment, and Lighting
**Goal**: The player can walk through a prison environment, collide with walls, and see warm lighting that renders correctly through the stylize pass
**Depends on**: Phase 1.1
**Requirements**: PLYR-01, PLYR-02, PLYR-03, ENVR-01, ENVR-02, ENVR-03
**Success Criteria** (what must be TRUE):
  1. The player can move through a gothic cathedral room using WASD and look around with the mouse — movement is smooth with no stutter
  2. The player cannot walk through walls, floors, or pillars — collision holds at edges and corners without getting stuck
  3. Light fixtures placed in the level cast visible warm illumination with correct attenuation and soft shadows
  4. A health indicator is visible on the HUD — it reads clearly against the stylized scene
  5. Prison environment geometry (walls, doors, furniture) loads from scene files and renders with correct materials
**Plans**: TBD
**UI hint**: yes

### Phase 02.1: Equipment Inventory and Carry Weight — weapons are inventory items, no slot-grid inventory, Dark Souls-style carry weight (INSERTED)

**Goal:** The player can open a paused, Dark Souls-style weapon inventory, equip owned weapons into explicit left-hand and right-hand slots, and see burden derived only from currently equipped weapons
**Requirements**: None (inserted gameplay-state phase; not mapped to `REQUIREMENTS.md` IDs)
**Depends on:** Phase 2
**Success Criteria** (what must be TRUE):
  1. Pressing `I` opens a dedicated paused inventory screen and closing it restores normal first-person control
  2. The inventory presents a category list, item list, detail pane, and visible burden/equip-load meter in a list-based layout
  3. Weapons are owned independently of being equipped, and only equipped weapons contribute to burden
  4. The player has explicit right-hand and left-hand weapon equipment, and equipping a two-handed weapon occupies both hands while clearing conflicts
  5. The phase does not add quick weapon cycling, non-weapon inventory, or a slot-grid/bag inventory model
**Plans:** 2/2 plans executed

Plans:
- [x] 02.1-01-PLAN.md — Define authored weapon metadata, owned-vs-equipped session state, and burden/equipment helper tests
- [x] 02.1-02-PLAN.md — Integrate the paused inventory system, list-based ImGui screen, and in-game verification

### Phase 3: Build menu in editor - macOS build from editor with progress and output

**Goal:** The level editor has a Build menu that invokes CMake to build the game executable, streams output to a dockable Build Output panel with progress and error highlighting, supports cancellation, and can build-and-run the game with the currently edited scene
**Requirements**: None (editor tooling phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 02.1
**Plans:** 2/2 plans complete

Plans:
- [x] 03-01-PLAN.md — EditorBuildSystem backend module (fork/exec, pipe, thread, SIGTERM) and runtime --scene argument
- [x] 03-02-PLAN.md — Wire Build menu, Build Output panel, keyboard shortcuts, unsaved-changes modal, and preferences into editor

### Phase 4: Improve lighting quality — research best practices and implement industry-standard real-time lighting

**Goal:** Upgrade the rendering pipeline to industry-standard real-time lighting: soft PCF shadows with 8 shadow slots, cascaded shadow maps for directional light, screen-space ambient occlusion, mip-chain bloom, rectangular area lights (LTC), tube lights, and emissive material support — all within OpenGL 4.1 Core Profile
**Requirements**: None (rendering quality phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 3
**Plans:** 5 plans

Plans:
- [ ] 04-01-PLAN.md — Soft shadow PCF upgrade (16-tap Poisson disk) and shadow slot expansion (2 to 8)
- [ ] 04-02-PLAN.md — Mip-chain bloom pipeline (13-tap downsample, tent upsample, replace old bloomGlow)
- [ ] 04-03-PLAN.md — Screen-space ambient occlusion (32-sample hemisphere SSAO with blur)
- [ ] 04-04-PLAN.md — Cascaded shadow maps for directional sun light (3 cascades, geometry shader)
- [ ] 04-05-PLAN.md — Area lights (LTC), tube lights (closest-point), and emissive material support
