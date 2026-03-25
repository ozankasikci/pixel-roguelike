# Roadmap: 3D Roguelike

## Overview

Build a first-person 3D roguelike with a custom C++ engine, starting with the defining 1-bit dithered rendering pipeline and working bottom-up through player movement, combat, enemies, and finally the game systems that make it shippable. Every phase delivers something runnable. The dithering aesthetic is validated first because everything else — lighting tuning, HUD readability, enemy silhouettes — depends on seeing output through the final visual filter.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Engine and Dithering Pipeline** - Custom C++ engine with OpenGL 4.1, the 1-bit Bayer dither post-process validated with world-space anchoring
- [ ] **Phase 1.1: Project Restructure** - ECS, Application class, modular engine/game split (INSERTED)
- [ ] **Phase 2: Player, Environment, and Lighting** - Player moves through a lit gothic level with collision; torches affect dither density
- [ ] **Phase 3: Game Systems** - Save, options menu, and game state transitions that make the experience shippable

## Phase Details

### Phase 1: Engine and Dithering Pipeline
**Goal**: The 1-bit dithered visual identity is working and visually correct — any geometry rendered through the pipeline produces the expected chunky black-and-white ordered dither pattern with no swimming on camera movement
**Depends on**: Nothing (first phase)
**Requirements**: RNDR-01, RNDR-02, RNDR-03, RNDR-04, RNDR-05
**Success Criteria** (what must be TRUE):
  1. A 3D scene renders to screen in pure black and white through the Bayer dither post-process — no grayscale, no color
  2. Moving the camera does not cause the dither pattern to crawl or swim — the pattern is anchored to world space
  3. The scene renders at a reduced internal resolution (403-720p) and upscales with nearest-neighbor, producing chunky visible dither cells rather than sub-pixel noise
  4. A torch-equivalent point light source visibly increases dither density (brighter region) in its radius compared to unlit geometry
  5. A Dear ImGui debug overlay lets the developer tune dither threshold and internal resolution at runtime without recompiling
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
**Goal**: The player can walk through a dithered gothic cathedral room, collide with walls, and see torch lighting that responds correctly through the dither pass
**Depends on**: Phase 1.1
**Requirements**: PLYR-01, PLYR-02, PLYR-03, ENVR-01, ENVR-02, ENVR-03
**Success Criteria** (what must be TRUE):
  1. The player can move through a gothic cathedral room using WASD and look around with the mouse — movement is smooth with no stutter
  2. The player cannot walk through walls, floors, or pillars — collision holds at edges and corners without getting stuck
  3. Torch objects placed in the level cast visible point light that affects the dither density, matching the expected 1-bit aesthetic
  4. A shape-based health indicator is visible on the HUD without using color — it reads clearly in 1-bit output
  5. Gothic cathedral geometry (arches, pillars, wall torches) loads from a model file and appears correctly dithered
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

### Phase 3: Game Systems
**Goal**: The game is a complete, shippable loop — the player can quit and resume progress, configure controls and display settings, and the game correctly transitions through menu, play, death, and level complete states
**Depends on**: Phase 3
**Requirements**: GSYS-01, GSYS-02, GSYS-03, GSYS-04
**Success Criteria** (what must be TRUE):
  1. The player can quit mid-run and resume from the last level boundary — progress is not lost on quit
  2. An options menu lets the player adjust FOV (03-120°), mouse sensitivity, and toggle fullscreen/windowed mode — changes take effect immediately
  3. The game correctly transitions between main menu, playing, paused, dead, and level complete states without getting stuck or crashing
**Plans**: TBD
**UI hint**: yes

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 1.1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Engine and Dithering Pipeline | 2/2 | Complete |  |
| 1.1. Project Restructure | 0/4 | Not started | - |
| 2. Player, Environment, and Lighting | 0/? | Not started | - |
| 3. Game Systems | 0/? | Not started | - |
