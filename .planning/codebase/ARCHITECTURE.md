# Architecture

**Analysis Date:** 2026-04-05

## Pattern Overview

**Overall:** Strict three-layer architecture with Engine → Game → Editor dependency flow. Game layer owns ECS state and gameplay logic; Engine provides low-level subsystems; Editor consumes both for scene authoring.

**Key Characteristics:**
- Type-safe service locator pattern via `Application` for cross-system access
- Phase-ordered system execution (Input → Interaction → Physics → Gameplay → Camera → Render)
- ECS-centric game logic with POD components and pure update functions
- RuntimeGameSession as autonomous game simulation container (used by both runtime and editor preview)
- Clear separation between serializable asset definitions (LevelDef, MaterialDefinition) and runtime state

## Layers

**Engine Layer (`src/engine/`):**
- Purpose: Low-level cross-platform abstractions for graphics, input, physics, audio, UI
- Exports subsystems as standalone libraries: `engine_core`, `engine_rendering`, `engine_input`, `engine_physics`, `engine_audio`, `engine_ui`, `engine_scene`
- Depends on: GLAD, GLFW, GLM, EnTT (core only), spdlog, Jolt Physics, OpenAL, Dear ImGui
- Used by: Game, Editor (via Game for shared functionality)
- Key constraint: Must NOT import from `game/` or `editor/` — exists independently

**Game Layer (`src/game/`):**
- Purpose: Roguelike-specific gameplay, content management, rendering pipeline, systems
- Three sub-libraries: `game_content` (data definitions) → `game_rendering` (scene visualization) → `gameplay` (systems & runtime)
- Depends on: All engine libraries, EnTT for ECS
- Used by: Editor, Runtime application
- Key responsibility: Owns `RuntimeGameSession` — complete simulation container for a play session or editor preview

**Editor Layer (`src/editor/`):**
- Purpose: Scene authoring, asset browsing, gizmo manipulation, layout/undo
- Consumes: Game layer directly (RuntimeGameSession for previews, LevelDef for serialization, ContentRegistry for asset resolution)
- Depends on: Game, Engine, ImGui, ImGuizmo, nlohmann_json
- Key constraint: Editor code never executes in the shipped game

## Layers: Detailed Breakdown

**Engine Core (`src/engine/core/`):**
- `Application.h` — Main loop orchestrator, system registry, phase-ordered execution, service locator
- `Window.h` — GLFW window creation/input callbacks
- `EventBus.h` — Type-safe pub/sub, subscription tokens for automatic cleanup
- `System.h` — Base class for phase-ordered systems (init/update/shutdown lifecycle)
- `Time.h` — Delta time tracking
- Services stored in `Application` via `emplaceService<T>()` / `getService<T>()`

**Engine Rendering (`src/engine/rendering/`):**
- **core/**: `Shader` (GLSL compilation + uniform caching), `Framebuffer` (FBO/RBO management)
- **geometry/**: `Mesh` (RAII VAO/VBO/EBO), `MeshLibrary` (named registry), `Renderer` (stateless draw dispatch)
- **assets/**: `AssimpLoader` (FBX/etc), `GltfLoader`, `Texture2D`, `TextureCube`, `AssetCache`
- **lighting/**: `RenderLight` (struct for spot/point/directional), `ShadowMap`, `CascadedShadowMap`, `ReflectionProbeRenderer`
- **post/**: `CompositePass`, `StylizePass`, `BloomPass`, `SsaoPass` (post-process chain)
- `SceneRenderPipeline.h` — Coordinates scene pass (PBR + shadows) and post-process passes

**Engine Input (`src/engine/input/`):**
- `InputSystem.h` — GLFW callback aggregation, ImGui integration, `RuntimeInputState` snapshot
- `ActionMap.h` — Key binding resolution

**Engine Physics (`src/engine/physics/`):**
- `PhysicsSystem.h` — Jolt Physics wrapper (pimpl'd to hide Jolt internals)
- Character controller API: `setCharacterVelocity()`, `updateCharacter()`, `getCharacterGroundState()`
- Dual interface: `init(Application&)` for Application phase system, `init(entt::registry&)` for RuntimeGameSession

**Engine Audio (`src/engine/audio/`):**
- `AudioSystem.h` — OpenAL Soft 3D audio dispatcher
- Integrated with `AudioSourceComponent` in game layer

**Engine UI (`src/engine/ui/`):**
- `ImGuiLayer.h` — ImGui initialization, theme/font presets, `DebugParams` struct
- `Screenshot.h` — Screenshot capture utilities
- **Issue**: `ImGuiLayer.h` violates layer boundary by including `game/rendering/RuntimeLightingOverride.h`

**Game Content (`src/game/content/`):**
- `ContentRegistry.h` — Central asset resolver for materials, environments, weapons, enemies, archetypes
- Loads from `assets/` directory, supports hot-reload via `pollMaterialHotReload()`
- Serialized format: JSON for definitions, GLSL procedural textures for materials

**Game Rendering (`src/game/rendering/`):**
- `RuntimeSceneRenderer.h` — Queries ECS for mesh/light/camera components, dispatches to `SceneRenderPipeline`
- `MaterialDefinition.h` — Shader parameter template with inheritance; resolves to `RenderMaterialData` at render time
- `MaterialTextureLibrary.h` — Deferred texture binding, GPU memory pooling
- `EnvironmentDefinition.h` — Sky, lighting presets, post-process overrides
- `EnvironmentProfile.h` — Legacy enum-based environment (deprecated, being phased out)

**Game Systems (`src/game/systems/`):**
- `PlayerMovementSystem.h` — Queries `PlayerMovementComponent`, calls `PhysicsSystem` API
- `CameraSystem.h` — Queries `CameraComponent`, `PrimaryCameraTag`; updates view/projection matrices
- `RenderSystem.h` — Game-layer system; calls `RuntimeSceneRenderer`, manages ImGui overlays
- `InteractionSystem.h` — Raycast-based door/object detection, prompt display
- `CheckpointSystem.h` — Checkpoint state tracking and respawn
- `InventorySystem.h` — Equipment and item management
- `AudioListenerSystem.h` — Queries `AudioListenerTag`, syncs OpenAL listener position
- **Pattern**: All game systems derive from `System`, registered in `Application` with phase ordering

**Game Components (`src/game/components/`):**
- All POD structs (no methods, no inheritance)
- Marker tags: `PlayerTag`, `ControllableTag`, `PrimaryCameraTag`, `AudioListenerTag`
- Data structs: `TransformComponent` (position/rotation/scale, computes modelMatrix()), `MeshComponent`, `LightComponent`, `CameraComponent`, `CharacterControllerComponent`, `StaticColliderComponent`, `InteractableComponent`, `DoorComponent`, `CheckpointComponent`, `PlayerMovementComponent`, `ViewmodelComponent`, `AudioSourceComponent`, `ReflectionProbeComponent`
- ECS queries use EnTT view patterns: `registry.view<PlayerTag, TransformComponent>()`

**Game Level Authoring (`src/game/level/`):**
- `LevelDef.h` — Serializable scene structure: placements of meshes, lights, colliders, archetypes, hierarchy
- `LevelLoader.h` — File I/O and ECS instantiation from `LevelDef`
- `LevelBuilder.h` — Factory for entities; used by loader and scripted geometry setup
- `LevelBuildContext.h` — Shared context during level construction (registry, content, mesh library)

**Game Runtime (`src/game/runtime/`):**
- `RuntimeGameSession.h` — **Complete simulation container**: owns registry, mesh library, physics, input, renderer, run session state. Can be instantiated standalone or in editor.
- Lifetime: `rebuild()` → `resetForPlay()` → `tick()/render()` loop → `clear()`
- `RuntimeGameplay.h` — Free functions for gameplay feature initialization/update (interaction, inventory, checkpoints, movement, camera)
- Used by: Runtime application (main game loop) and Editor (preview world)

**Game Behavior (`src/game/behavior/`):**
- `ActionTypes.h` — Enum + variant-based action system for door opening, sound playing, messages, delays
- `BehaviorSystem.h` — Executes actions triggered by collider/entity events
- `DoorAnimationSystem.h` — Animates door leaf rotation over time

**Game Prefabs (`src/game/prefabs/`):**
- `GameplayPrefabs.h` — Factory functions for composite entities (checkpoint with collider + visual, double door with linked leaves)
- `GameplayPrefabData.h` — Data structures for prefab specifications

**Editor Scene (`src/editor/scene/`):**
- `EditorSceneDocument.h` — In-memory scene graph with parent/child hierarchy, world transform calculation, serialization
- `EditorSceneObject` — Variant type holding mesh/light/collider/archetype/group placements
- Conversion: `EditorSceneDocument` ↔ `LevelDef` ↔ `.scene` file (JSON)
- `EditorSelectionSystem.h` — Picking and multi-select
- `EditorPreviewWorld.h` — Spawns `RuntimeGameSession` from `EditorSceneDocument` for live preview

**Editor Viewport (`src/editor/viewport/`):**
- `EditorViewportController.h` — Camera orbit control
- ImGuizmo gizmo integration for translate/rotate/scale
- `EditorViewportInteraction.h` — Gizmo drag state machine

**Editor UI (`src/editor/ui/`):**
- `LevelEditorUi.h` — ImGui windows: outliner, inspector, asset browser, environment panel
- Editable fields with undo/redo integration

**Editor Core (`src/editor/core/`):**
- `LevelEditorCore.h` — Dock layout management, scene loading, layout presets
- `EditorRuntimePreviewSession.h` — Wraps `RuntimeGameSession`, syncs input/camera/environment from editor UI
- `EditorCommandStack.h` — Undo/redo implementation

**Editor Debug Harness (`src/editor/debug/`):**
- Unix socket remote control system at `/tmp/pixel-roguelike-editor-{pid}.sock`
- 33 commands for inspection and mutation (see project memory: `editor_debug_harness.md`)

## Data Flow

**Level Loading (Runtime):**
1. File on disk: `assets/scenes/{scene}.scene` (JSON)
2. `LevelLoader::load()` → deserialize to `LevelDef` struct
3. `LevelBuilder` iterates `LevelDef` placements, spawns ECS entities with components
4. `ContentRegistry` resolves mesh IDs, material IDs, archetype definitions
5. `PhysicsSystem` collects `StaticColliderComponent` entities, initializes Jolt bodies
6. `RuntimeSceneRenderer` reads `MeshComponent`, `LightComponent`, `CameraComponent` from registry each frame

**Rendering (Per Frame):**
1. `RenderSystem` calls `RuntimeSceneRenderer::render()`
2. `RuntimeSceneRenderer` queries: `registry.view<MeshComponent, TransformComponent>()`
3. Collects `RenderObject` (mesh, modelMatrix, material) into scratch vectors
4. Passes to `SceneRenderPipeline`:
   - Shadow depth pass (per light)
   - Scene pass (PBR, samples shadow maps, 32 max lights)
   - Post-process: stylize, composite to screen
5. ImGui overlay pass (debug params, prompts, HUD)

**Material Resolution:**
1. `MaterialDefinition` loaded from JSON with inheritance chain
2. At render time: `MaterialTextureLibrary` binds albedo/normal/roughness/AO textures to GPU
3. Shader receives `RenderMaterialData` with booleans for detail/subsurface/etc
4. Procedural textures generated in shader for patterns (brick, stone, wood, floor)

**Editor Round-Trip:**
1. User edits in `EditorSceneDocument` (in-memory hierarchy)
2. Changes published to `EditorRuntimePreviewSession` which owns a `RuntimeGameSession`
3. Preview session's registry updated with new components/entities
4. Save: `EditorSceneDocument` serialized to `LevelDef` JSON
5. Load: JSON deserialized to `EditorSceneDocument`

**Behavior Execution:**
1. `BehaviorSystem` queries entities with `BehaviorDeclaration` vectors
2. On trigger (collider contact, timer, event): enqueue `ActionEntry`
3. Dispatch: variant-based switch on `ActionType` (OpenDoor, PlaySound, etc)
4. Example: `ActionType::OpenDoor` → find `DoorComponent` by `targetNodeId`, set state
5. `DoorAnimationSystem` animates the leaf rotation based on door state

## Key Abstractions

**Application (Service Locator):**
- Purpose: Centralized access to systems and singletons
- Examples: `app.getService<InputSystem>()`, `app.getService<ContentRegistry>()`
- Pattern: `std::unordered_map<std::type_index, std::any>` with template accessors
- Eliminates spaghetti global state; enforces explicit dependency

**System (Phase-Ordered Execution):**
- Purpose: Decouples game logic from main loop scheduling
- Lifecycle: `init(app)` once, `update(app, dt)` per frame, `shutdown()` on exit
- Examples: `PlayerMovementSystem`, `CameraSystem`, `RenderSystem`
- Phase enum ensures deterministic order: Input → Physics → Gameplay → Render

**EventBus (Type-Safe Pub/Sub):**
- Purpose: Decouples event producers from consumers
- Subscription tokens auto-unsubscribe on destruction (RAII)
- Not currently heavily used in gameplay systems (direct ECS queries preferred)

**RuntimeGameSession (Simulation Container):**
- Purpose: Self-contained play session or editor preview
- Owns: ECS registry, mesh library, physics, input, renderer
- Allows: Multiple independent game instances (e.g., editor with background preview)
- Lifetime: Call `rebuild(levelDef, content)` → `resetForPlay()` → `tick()/render()` → `clear()`

**ECS Component Queries:**
- Pattern: `auto view = registry.view<ComponentA, ComponentB>();` via EnTT
- Sparse-set storage guarantees cache-friendly iteration
- Example: `registry.view<PlayerTag, TransformComponent>()` finds player entity

**Content Registry (Asset Resolver):**
- Purpose: Central authority for mesh, material, environment, prefab definitions
- Hot-reload support for editor iteration
- Validation: Material inheritance chains checked on load

## Entry Points

**Runtime Application (`apps/runtime/main.cpp`):**
- Creates `Application` (GLFW window, ECS registry)
- Registers game systems: `PlayerMovementSystem`, `CameraSystem`, `RenderSystem`, etc.
- Loads level via `RuntimeGameSession::rebuild()`
- Main loop: `app.run()` → phases → `RenderSystem::update()` → `window.swap()`

**Level Editor (`apps/level_editor/main.cpp`):**
- Creates `Application` for ImGui context
- `LevelEditorCore` manages scene document and preview world
- ImGui dockspace layout with viewport, outliner, inspector, asset browser
- Hot-reload: material changes + scene preview updates in real-time

**Procedural Model Viewer (`apps/model_viewer/main.cpp`):**
- Minimal application for testing procedural geometry generation
- Does not need full gameplay systems

## Error Handling

**Strategy:** Exceptions for initialization failures; assertions for runtime invariants

**Patterns:**
- Asset loading: throws on file not found, parse errors → caught and logged at startup
- Physics: assertions on invalid shapes or character controller state
- Rendering: GL error checks in debug builds via `glGetError()`
- ECS: assertions on invalid entity/component access (EnTT checks)

**In Editor:** Validation warnings displayed in ImGui windows (invalid material inheritance, missing mesh)

## Cross-Cutting Concerns

**Logging:** spdlog with category filters. Examples: `SPDLOG_INFO("scene_loader")`, `SPDLOG_WARN("physics")`

**Validation:** 
- Materials: inheritance chains validated on load
- Levels: nodeId references checked (orphaned children logged as warnings)
- Archetypes: prefab instantiation validates component slots

**Authentication/Authorization:** Not applicable (single-player game)

**Thread Safety:** Main thread only. Physics and audio run in background threads via their respective libraries (Jolt, OpenAL), but game layer never touches them concurrently.

---

*Architecture analysis: 2026-04-05*
