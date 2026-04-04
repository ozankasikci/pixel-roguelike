# Roadmap: 3D Roguelike

## Overview

Build a first-person 3D psychological horror game with a custom C++ engine, starting with the rendering pipeline and working bottom-up through player movement, combat, enemies, and finally the game systems that make it shippable. Every phase delivers something runnable. The Stanley Parable-inspired visual style (clean surfaces, warm lighting, muted palette) is the foundation that all gameplay builds on.

## Milestones

- v1.0 — Phases 1-8 (shipped 2026-04-01)
- v1.1 Editor UX — Phases 9-11 (in progress)

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

<details>
<summary>v1.0 — Phases 1-8 (shipped 2026-04-01)</summary>

- [x] **Phase 1: Engine and Rendering Pipeline** - Custom C++ engine with OpenGL 4.1, stylize post-process with edge detection and tone mapping
- [x] **Phase 1.1: Project Restructure** - ECS, Application class, modular engine/game split (INSERTED)
- [x] **Phase 2: Player, Environment, and Lighting** - Player moves through a lit prison environment with collision; warm lighting and material system
- [x] **Phase 2.1: Equipment Inventory** - Dark Souls-style weapon inventory with equip slots and carry weight (INSERTED)
- [x] **Phase 3: Build Menu in Editor** - macOS build from editor with progress and output
- [x] **Phase 4: Improve Lighting Quality** - Industry-standard real-time lighting: PCF shadows, bloom, SSAO, CSM, LTC area lights
- [x] **Phase 5: Unify Editor/Runtime/Build Rendering Parity** - Shared render path so all three modes produce identical visual output
- [x] **Phase 6: Data-driven Scene Management** - New/Delete Scene in editor, configurable runtime default, remove legacy scene classes
- [x] **Phase 7: Data-driven Material System** - Replace hardcoded materials with a proper material pipeline
- [x] **Phase 8: Create Institutional Room Scene** - Institutional room from concept art with warm beige walls, fluorescent panels, three doors, and interaction stubs

</details>

### v1.1 Editor UX

**Milestone Goal:** Bring the level editor to professional quality — matching Unity/Unreal workflows for scene object manipulation.

- [x] **Phase 9: Selection Overlay Depth Fix** - Depth-correct selection highlight that does not bleed through occluding geometry (completed 2026-04-01)
- [x] **Phase 10: Global Keyboard Shortcuts and Hover Highlight** - Delete, Ctrl+D, Escape, F, and hover feedback all work from any editor panel (completed 2026-04-01)
- [x] **Phase 11: Add Mesh Discoverability** - Mesh picker button lets the user add meshes to the scene without knowing keyboard shortcuts (completed 2026-04-01)

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
- [x] 04-01-PLAN.md — Soft shadow PCF upgrade (16-tap Poisson disk) and shadow slot expansion (2 to 8)
- [x] 04-02-PLAN.md — Mip-chain bloom pipeline (13-tap downsample, tent upsample, replace old bloomGlow)
- [x] 04-03-PLAN.md — Screen-space ambient occlusion (32-sample hemisphere SSAO with blur)
- [x] 04-04-PLAN.md — Cascaded shadow maps for directional sun light (3 cascades, geometry shader)
- [ ] 04-05-PLAN.md — Area lights (LTC), tube lights (closest-point), and emissive material support

### Phase 5: Unify editor/runtime/build rendering parity

**Goal:** Extract a shared SceneRenderPipeline from RuntimeSceneRenderer so the editor viewport, runtime game, and model viewer all render identically through the same code path — including bloom, SSAO, cascaded shadow maps, LTC area lights, tube lights, and emissive materials
**Requirements**: None (rendering architecture phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 4
**Plans:** 3/3 plans complete

Plans:
- [x] 05-01-PLAN.md — Extract SceneRenderPipeline into engine_rendering and refactor RuntimeSceneRenderer to compose it
- [x] 05-02-PLAN.md — Create EditorViewportRenderer, wire editor to shared pipeline, extend environment panel
- [x] 05-03-PLAN.md — Wire model viewer to shared pipeline, add pipeline stats, update asset preview LTC

### Phase 6: Data-driven scene management

**Goal:** Add "New Scene" and "Delete Scene" to the editor UI, make the runtime default scene configurable instead of hardcoded, and remove legacy scene classes (CathedralScene, SilosCloisterScene, WardenOfficeScene) in favor of GenericFileScene
**Requirements**: None (editor tooling phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 5
**Plans:** 3/3 plans complete

Plans:
- [x] 06-01-PLAN.md — Consolidate asset registration into registerAllGameAssets, delete legacy scenes, create ProjectConfig utilities
- [x] 06-02-PLAN.md — Wire project.cfg into runtime and editor startup, runtime scene picker, empty editor state
- [x] 06-03-PLAN.md — New Scene, Delete, Rename operations, asset browser scene enhancements, unsaved changes guard

### Phase 7: Data-driven material system — replace hardcoded materials with a proper material pipeline

**Goal:** Replace the hardcoded MaterialKind enum and material registration with a fully data-driven material pipeline: auto-discover materials from the filesystem, remove the MaterialKind enum in favor of property-driven PBR shading with feature flags, refactor the scene shader to a single uber-shader, and add editor tooling for creating/editing/previewing materials with hot-reload
**Requirements**: None (material pipeline phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 6
**Plans:** 4 plans

Plans:
- [x] 07-01-PLAN.md — Add feature flags to MaterialDefinition, replace hardcoded material list with auto-scanner, bake shader defaults into .material files, migrate .scene files
- [x] 07-02-PLAN.md — Delete MaterialKind enum, refactor scene.frag/scene.vert to property-driven uber-shader, update all call sites
- [x] 07-03-PLAN.md — Editor material browser category, inspector panel with feature flags and preview sphere, CRUD operations
- [x] 07-04-PLAN.md — File watcher hot-reload, material validation at load/save, per-material cache invalidation

### Phase 8: Create institutional room scene from concept art

**Goal:** Build a complete institutional room scene from concept art: an 8m x 6m Stanley Parable-inspired corridor room with warm beige walls, dark brown trim, glossy floor, fluorescent ceiling panels, and three doors on the far wall (open wooden door with warm light, locked gray metal door with HVAC vent, locked white chained door with padlock) plus props (smoke detector with red LED)
**Requirements**: None (content creation phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 7
**Plans:** 2 plans

Plans:
- [x] 08-01-PLAN.md — Create materials (inst_beige_wall, inst_glossy_floor, inst_dark_trim), environment profile (institutional), and three procedural meshes (HVAC vent, smoke detector, chain/padlock)
- [x] 08-02-PLAN.md — Assemble the institutional_room.scene file with all geometry/lighting/colliders/props, wire locked door interaction stubs, visual verification

### Phase 9: Selection Overlay Depth Fix

**Goal:** The selection highlight renders correctly in 3D space — occluding geometry blocks the overlay rather than the wireframe bleeding through walls and objects in front of the selection
**Depends on:** Phase 8
**Requirements**: SEL-01
**Success Criteria** (what must be TRUE):
  1. Selecting an object behind a wall shows the selection outline only where the object is actually visible — the outline does not bleed through the wall
  2. Selecting a fully occluded object shows a faint ghost outline at reduced opacity, confirming the selection without obscuring foreground geometry
  3. Selecting objects at varying distances and overlap configurations all produce correct depth-respecting outlines
**Plans**: 1 plan

Plans:
- [x] 09-01-PLAN.md — Two-pass selection overlay: depth-tested primary wireframe + dim ghost for occluded objects

### Phase 10: Global Keyboard Shortcuts and Hover Highlight

**Goal:** The editor responds to Delete, Ctrl+D, Escape, and F from any focused panel — viewport, outliner, or inspector — and shows a hover highlight on objects under the cursor before they are clicked
**Depends on:** Phase 9
**Requirements**: SEL-02, SEL-03, OBJ-01, OBJ-02, OBJ-03
**Success Criteria** (what must be TRUE):
  1. Pressing Delete with an object selected removes it from both the viewport and the outliner regardless of which panel is focused — pressing Delete inside a text field does not remove scene objects
  2. Pressing Ctrl+D duplicates the selected object with a visible position offset and transfers selection to the new copy
  3. Pressing Escape with any selection active clears the selection from all panels
  4. Pressing F with an object selected smoothly frames the viewport camera on that object
  5. Moving the cursor over an unselected object in the viewport shows a visible highlight before clicking
**Plans**: 2 plans

Plans:
- [x] 10-01-PLAN.md — Keyboard shortcut guards (Delete/F/Escape with WantTextInput), Escape clears selection, duplicate offset, smooth animated multi-select camera framing
- [x] 10-02-PLAN.md — Hover highlight overlay (blue-white wireframe on unselected objects), per-frame raycast, visual verification

### Phase 11: Add Mesh Discoverability

**Goal:** A clearly labeled button in the editor lets the user pick a mesh from the project's asset library and place it into the current scene — no keyboard shortcut knowledge required
**Depends on:** Phase 10
**Requirements**: DISC-01
**Success Criteria** (what must be TRUE):
  1. An "Add Mesh" button is visible in the editor without needing to right-click or know any shortcut
  2. Clicking the button opens a picker showing all available meshes in the project
  3. Selecting a mesh from the picker places it in the scene at a sensible default position and selects it immediately
  4. The newly placed mesh appears in both the viewport and the outliner and is saved when the scene is saved
**Plans**: 1 plan

Plans:
- [x] 11-01-PLAN.md — Add Mesh toolbar button with filtered popup picker, commitPlacement return type extension, auto-select after placement

### Phase 12: Engine quality: frustum culling, texture unit enum, generic asset system, EventBus RAII tokens

**Goal:** Seven internal engine-quality improvements: AABB frustum culling in the shared render pipeline, named texture unit enum replacing magic numbers, file-based mesh auto-discovery matching the material pattern, EventBus RAII subscription tokens, DebugParams decomposition, LevelLoader unification, and GenericFileScene scripted-geometry extraction
**Requirements**: None (engine quality phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 11
**Success Criteria** (what must be TRUE):
  1. Objects outside the camera frustum are culled before draw submission — draw call count drops when looking away from geometry
  2. All texture unit magic numbers (8-16) are replaced with named constants from TextureUnits.h
  3. New mesh files placed in assets/meshes/ are automatically available without code changes
  4. EventBus::subscribe() returns an RAII token that unsubscribes on destruction
  5. DebugParams is decomposed into focused sub-structs (CameraDebugInfo, RuntimeLightingOverride)
  6. LevelLoader has a single unified load() overload
  7. GenericFileScene has no hard-coded level-specific if-chains
**Plans:** 4/4 plans complete

Plans:
- [x] 12-01-PLAN.md — TextureUnits.h named constants and EventBus RAII SubscriptionToken
- [x] 12-02-PLAN.md — DebugParams decomposition into CameraDebugInfo and RuntimeLightingOverride
- [x] 12-03-PLAN.md — ProceduralGameAssets rename, LevelLoader unification, GenericFileScene scripted geometry extraction
- [x] 12-04-PLAN.md — File-based mesh auto-discovery and AABB frustum culling in SceneRenderPipeline

### Phase 13: Data-driven behavior system — native Action Component system with BehaviorSystem dispatcher, TriggerComponent, node ID targeting, scene file behavior declarations

**Goal:** Replace per-behavior System classes (DoorSystem, CheckpointSystem) with a single BehaviorSystem that dispatches actions from data-driven BehaviorComponent action lists, add TriggerComponent for spatial trigger volumes with on_enter/on_exit events, extend the .scene file format with indented sub-lines for behavior declarations, build a NodeIndex for entity name resolution, and migrate existing scenes to use the new behavior system
**Requirements**: None (gameplay architecture phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 12
**Success Criteria** (what must be TRUE):
  1. A single BehaviorSystem dispatches all entity behaviors from BehaviorComponent action lists -- no per-behavior System classes for activation
  2. The .scene file format supports indented sub-lines for interactable and behavior declarations on mesh/light/trigger entities
  3. TriggerComponent with Box and Sphere shapes fires onEnter/onExit action lists when the player enters/exits the volume
  4. NodeIndex resolves "self" and named node IDs to entities at load time for cross-entity action targeting
  5. DoorSystem is replaced by DoorAnimationSystem (animation only, no activation logic)
  6. Institutional room doors are defined in the .scene file with interactable sub-lines (not scripted geometry)
  7. Editor renders trigger volumes as semi-transparent wireframes for level design positioning
**Plans:** 2/3 plans executed

Plans:
- [x] 13-01-PLAN.md — Behavior type definitions (ActionTypes, BehaviorComponent, TriggerComponent, NodeIndex) and scene parser extension with indented sub-line support
- [x] 13-02-PLAN.md — BehaviorSystem dispatcher, TriggerSystem overlap detection, DoorAnimationSystem refactor, runtime wiring
- [ ] 13-03-PLAN.md — Scene file migration, scripted geometry cleanup, editor trigger visualization, end-to-end verification

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 1.1 → 2 → 2.1 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Engine and Rendering Pipeline | v1.0 | 2/2 | Complete | 2026-03-23 |
| 1.1. Project Restructure | v1.0 | 4/4 | Complete | 2026-03-24 |
| 2. Player, Environment, and Lighting | v1.0 | TBD | Not started | - |
| 2.1. Equipment Inventory | v1.0 | 2/2 | Complete | 2026-03-25 |
| 3. Build Menu in Editor | v1.0 | 2/2 | Complete | 2026-03-26 |
| 4. Improve Lighting Quality | v1.0 | 4/5 | In progress | - |
| 5. Unify Rendering Parity | v1.0 | 3/3 | Complete | 2026-03-28 |
| 6. Data-driven Scene Management | v1.0 | 3/3 | Complete | 2026-03-29 |
| 7. Data-driven Material System | v1.0 | 4/4 | Complete | 2026-03-30 |
| 8. Institutional Room Scene | v1.0 | 2/2 | Complete | 2026-04-01 |
| 9. Selection Overlay Depth Fix | v1.1 | 1/1 | Complete   | 2026-04-01 |
| 10. Global Keyboard Shortcuts and Hover Highlight | v1.1 | 2/2 | Complete    | 2026-04-01 |
| 11. Add Mesh Discoverability | v1.1 | 1/1 | Complete    | 2026-04-01 |
| 12. Engine Quality | — | 4/4 | Complete    | 2026-04-01 |
| 13. Data-driven Behavior System | — | 1/3 | In Progress|  |

### Phase 14: improve lighting, reflections, occlusion, and shadow quality

**Goal:** Upgrade the renderer from a strong direct-lighting base into a polished, performant lighting stack for this game: real sky-driven reflections, indoor reflection probes where needed, cleaner occlusion, more stable shadows, and editor preview quality controls that keep fullscreen play responsive
**Requirements**: None (rendering polish phase; not mapped directly to `REQUIREMENTS.md` IDs)
**Depends on:** Phase 13
**Success Criteria** (what must be TRUE):
  1. Glossy and metallic surfaces show believable environment reflections from sky/probe data rather than only direct-light highlights
  2. Interior rooms can use local reflection probes with box projection for more convincing room-shaped reflections
  3. Ambient occlusion remains subtle, tunable, and cheaper to run than the current fixed full-resolution path
  4. Directional shadows are visibly more stable and tunable in large fullscreen views
  5. Fullscreen editor play preview exposes render scale / quality controls and no longer forces a single expensive full-resolution rendering path
**Plans:** 3 plans

Plans:
- [ ] 14-01-PLAN.md — Add global specular IBL from the existing sky cubemap path
- [ ] 14-02-PLAN.md — Add local box-projected reflection probes and scene/editor authoring support
- [ ] 14-03-PLAN.md — Improve SSAO/shadow quality and add preview quality/render-scale controls

### Phase 15: prototype editor ImGui themes and choose a final skin direction

**Goal:** The level editor supports three live-switchable Dear ImGui theme presets, persists the chosen interface font/theme locally, and lands on a warm polished default skin direction that matches the project's muted institutional art style
**Requirements**: None (editor chrome/polish phase; not mapped to `REQUIREMENTS.md` IDs)
**Depends on:** Phase 14
**Plans:** 1/2 plans executed

Plans:
- [x] 15-01-PLAN.md — Add ImGui theme preset infrastructure and a live View -> Interface Theme selector
- [ ] 15-02-PLAN.md — Persist editor font/theme preferences locally and verify the final default skin in-editor

### Phase 16: Editor trigger and behavior authoring with save-load fidelity

**Goal:** Make triggers first-class editor objects and add UI for authoring behaviors and interactables on scene entities, with full round-trip save-load fidelity through toLevelDef() and loadFromSceneFile()
**Requirements**: None (editor tooling phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 15
**Plans:** 2/3 plans executed

Plans:
- [x] 16-01-PLAN.md — Promote triggers to first-class EditorSceneObject entries, remove readOnlyTriggers_, update all switch sites, outliner context menu, round-trip test
- [x] 16-02-PLAN.md — Inspector UI for trigger properties, behavior editing sections with categorized action dropdown, interactable editing
- [ ] 16-03-PLAN.md — Trigger resize gizmo handles, interaction distance ring, showTriggers toggle, visual verification

### Phase 17: Unified collider system with trigger capabilities and scripting support

**Goal:** Replace the two separate collision systems (StaticColliderComponent with Jolt physics bodies and TriggerComponent with manual AABB/sphere overlap) with a single unified ColliderComponent supporting Solid, Trigger, and SolidAndTrigger modes, all backed by Jolt Physics sensor bodies for trigger detection
**Requirements**: None (architecture unification phase; not mapped to REQUIREMENTS.md IDs)
**Depends on:** Phase 16
**Success Criteria** (what must be TRUE):
  1. A single ColliderComponent with ColliderMode (Solid/Trigger/SolidAndTrigger) replaces both StaticColliderComponent and TriggerComponent
  2. All four shape types (Box, Sphere, Cylinder, Capsule) work in all three modes
  3. Trigger detection uses Jolt sensor bodies via CharacterContactListener, not manual AABB/sphere overlap
  4. Scene files use unified `collider <shape> <mode>` syntax with backward-compatible parsing of old format
  5. Editor has a single Collider kind with shape and mode dropdowns, replacing three separate object kinds
  6. TriggerSystem is deleted; BehaviorSystem reads flags from ColliderComponent
  7. All scene files migrated, all tests pass, all executables build
**Plans:** 3/3 plans executed

Plans:
- [x] 17-01-PLAN.md — Core types (ColliderComponent), unified LevelDef placement, parser/serializer update, scene file migration
- [x] 17-02-PLAN.md — PhysicsSystem sensor bodies via CharacterContactListener, BehaviorSystem update, TriggerSystem deletion, runtime wiring
- [x] 17-03-PLAN.md — Editor unification: single Collider kind, inspector shape/mode dropdowns, preview renderer, all panel updates
