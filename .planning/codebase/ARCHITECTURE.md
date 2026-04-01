# Architecture

**Analysis Date:** 2026-04-01

## Pattern Overview

**Overall:** Layered monorepo with explicit dependency direction — Engine → Game → Editor — enforced by CMake target graph. Within each layer, an Entity-Component System (EnTT) is used for game state, with free-function systems operating on typed component views rather than OOP objects owning their own logic.

**Key Characteristics:**
- Strict layer boundaries: engine has zero game/editor includes; game includes engine only; editor includes game and engine
- ECS for runtime game state (EnTT `entt::registry` owned by `Application` or `RuntimeGameSession`)
- Phase-ordered system execution registered at boot time; not a runtime scheduler
- Type-erased service locator (`std::any` map keyed by `std::type_index`) for cross-cutting singletons
- Rendering is fully decoupled from ECS — renderer collects `RenderObject`/`RenderLight` structs from the registry each frame and hands them to a pipeline that has no ECS dependency

**Comparison to known engines:**
- Closer to **Hazel/TheCherno** than Godot: no scene-tree node hierarchy, no signal system, explicit system registration in `main.cpp`
- Unlike **Bevy**, systems are not auto-discovered via reflection; registration is manual and phase assignment is explicit
- Unlike **Godot**, there is no built-in node ownership model — entity lifetime is managed by scenes and tracked in `std::vector<entt::entity>`
- Similar to **raylib** in keeping `main()` as the wiring point; unlike raylib there is a proper System abstraction with `init/update/shutdown` lifecycle

---

## Layers

**Engine Layer:**
- Purpose: Platform abstraction, core loop, rendering pipeline, physics, input, ECS registry wrapper
- Location: `src/engine/`
- Contains: `Application`, `Window`, `EventBus`, `System`, `SceneManager`, `InputSystem`, `PhysicsSystem`, `AudioSystem`, `ImGuiLayer`, mesh/shader/texture RAII classes, all post-process passes, `SceneRenderPipeline`
- Depends on: GLFW, EnTT, GLM, GLAD, Jolt Physics, OpenAL Soft, Dear ImGui, spdlog
- Used by: Game layer, Editor layer, all three app executables

**Game Layer:**
- Purpose: Game-specific components, systems, content loading, level management, runtime session
- Location: `src/game/`
- Contains: All POD components, `RuntimeGameSession`, `RuntimeSceneRenderer`, `ContentRegistry`, `LevelDef`/`LevelLoader`/`LevelBuilder`, `GameplayPrefabs`, `RunSession`, game UI overlays
- Depends on: Engine layer only
- Used by: Editor layer (preview sessions), runtime app, level editor app

**Editor Layer:**
- Purpose: Level editor tooling — scene document model, undo/redo, viewport, inspector, asset browser
- Location: `src/editor/`
- Contains: `EditorSceneDocument`, `EditorCommandStack`, `EditorPreviewWorld`, `EditorRuntimePreviewSession`, `LevelEditorCore`, `EditorViewportController`, all editor UI panels
- Depends on: Game layer and Engine layer
- Used by: `apps/level_editor/` only

---

## Subsystems

### ECS (Entity-Component System)

The `entt::registry` lives in `Application` for the runtime game (accessed via `app.registry()`), and as a private member of `RuntimeGameSession` and `EditorPreviewWorld` for isolated editor contexts.

**Components** — all POD structs, no methods except computed properties:
- `src/game/components/TransformComponent.h` — position/rotation/scale + `modelMatrix()` helper
- `src/game/components/MeshComponent.h` — non-owning `Mesh*` + materialId + tint
- `src/game/components/LightComponent.h`, `CameraComponent.h`, `CharacterControllerComponent.h`
- `src/game/components/DoorComponent.h`, `DoorLeafComponent.h`, `InteractableComponent.h`, `CheckpointComponent.h`
- `src/game/components/PlayerTag.h`, `PrimaryCameraTag.h`, `ControllableTag.h`, `AudioListenerTag.h` — zero-size marker types
- `src/game/components/PlayerMovementComponent.h`, `ViewmodelComponent.h`, `StaticColliderComponent.h`

Components follow the **Bevy/EnTT data-oriented** idiom: no virtual methods, no inheritance, no back-pointer to owning entity. Systems are the only code that reads/writes components.

**Systems** — inherit from `System` base class in `src/engine/core/System.h`:
```cpp
class System {
public:
    virtual void init(Application& app) = 0;
    virtual void update(Application& app, float deltaTime) = 0;
    virtual void shutdown() = 0;
};
```

Unlike Godot nodes or Unity MonoBehaviours, systems are not per-entity. One system instance processes all entities with matching components via EnTT views.

### System Execution Model

Systems are registered by phase in `apps/runtime/main.cpp` and stored in `Application::systemsByPhase_` — a `std::array<std::vector<unique_ptr<System>>, 6>`:

```
Phase 0: Input       → InputSystem
Phase 1: Interaction → InteractionSystem, DoorSystem, CheckpointSystem
Phase 2: Physics     → PhysicsSystem
Phase 3: Gameplay    → AudioSystem, AudioListenerSystem, InventorySystem, PlayerMovementSystem
Phase 4: Camera      → CameraSystem
Phase 5: Render      → RenderSystem
```

The main loop in `Application::run()` iterates all phases in order each frame. Shutdown runs in reverse phase order. There is no dependency graph or parallel execution — single-threaded, sequential, deterministic.

This differs from **Bevy's** scheduler (which can parallelize independent systems automatically) and **Godot's** `_process/_physics_process` split, but is simpler to reason about and debug.

### Service Locator

`Application` provides a type-safe service locator backed by `std::unordered_map<std::type_index, std::any>`:

```cpp
// Registration (in main.cpp)
auto& content = app.emplaceService<ContentRegistry>();

// Retrieval (in any system that receives Application&)
ContentRegistry& content = app.getService<ContentRegistry>();

// Safe retrieval
ContentRegistry* content = app.tryGetService<ContentRegistry>();
```

Services registered: `RunSession`, `ContentRegistry`, `AudioSystem*`.

This is a classic **service locator** pattern. Unlike Godot's singleton nodes, services are not globally accessible — only code holding an `Application&` reference can reach them. This is stricter than a global singleton but looser than pure dependency injection.

**Registry context** is also used for some per-registry singletons:
```cpp
app.registry().ctx().insert_or_assign<ContentRegistry*>(&content);
```
This pattern appears in `GenericFileScene` and `LevelLoader` for data that needs to be accessible from inside ECS systems without passing `Application&` everywhere.

---

## Rendering Pipeline

The rendering stack has two layers: engine (`SceneRenderPipeline`) and game (`RuntimeSceneRenderer`).

### RuntimeSceneRenderer (game layer) — `src/game/rendering/RuntimeSceneRenderer.h`

Queries ECS each frame and produces engine-layer structs:

1. **`captureCamera()`** — finds entity with `PrimaryCameraTag`, reads `CameraComponent`, builds view/projection matrices
2. **`collectSceneObjects()`** — queries entities with `TransformComponent + MeshComponent`, resolves `materialId` via `MaterialTextureLibrary`, produces `std::vector<RenderObject>`
3. **`collectViewmodelObjects()`** — queries `ViewmodelComponent` entities; applies first-person camera-space positioning
4. **`collectLights()`** — queries entities with `LightComponent`, produces `std::vector<RenderLight>`
5. Packs everything into a `SceneRenderInput` struct and calls `SceneRenderPipeline::render()`

This follows the **push-data** pattern (similar to how TheCherno's Hazel engine works): ECS state is extracted into plain renderer input structs with no ECS types crossing the engine/game boundary.

### SceneRenderPipeline (engine layer) — `src/engine/rendering/SceneRenderPipeline.h`

Has zero game-layer dependencies. Orchestrates the full GPU pipeline:

```
1. Shadow pass    → spot light shadow maps (up to 8)   — shadow_depth.vert/frag
2. CSM pass       → cascaded shadow map for sun         — csm_depth.vert/geom/frag
3. Scene pass     → PBR-like lighting to sceneFBO_      — game/scene.vert/frag (42KB)
4. Bloom pass     → dual-pass downsample/upsample        — bloom_downsample, bloom_upsample
5. SSAO pass      → screen-space ambient occlusion       — ssao.vert/frag, ssao_blur
6. Composite pass → tone map + fog + sky + SSAO blend   → compositeFBO_   — composite.frag (12KB)
7. Stylize pass   → edge detection + vignette + grain    → targetFramebuffer — stylize.frag
```

The scene shader at `assets/shaders/game/scene.frag` (42KB) handles: PBR (metalness/roughness), 32 point/spot/area lights, LTC area light approximation, cascaded shadow maps, procedural textures (brick, stone, wood, floor, ceiling), subsurface scattering approximation, animated materials.

**Post-process chain** is configured via `PostProcessParams` (30+ toggleable features). `EnvironmentDefinition` wraps `PostProcessParams + SkySettings + LightingEnvironment` and is serialized to `.environment` files.

### Editor rendering

The editor has its own renderer path at `src/editor/render/EditorScenePreviewRenderer.h` that:
- Uses the same `SceneRenderPipeline` (shared engine layer)
- Adds selection overlays, collider wireframes, light indicators as extra `RenderObject`s
- `EditorRuntimePreviewSession` wraps `RuntimeGameSession` for in-editor play-testing

---

## Scene / Level System

**Scene stack** (`SceneManager`) — pushes/pops `Scene` interface instances:
```cpp
class Scene {
    virtual void onEnter(Application& app) = 0;
    virtual void onExit(Application& app) = 0;
    virtual void onUpdate(Application& app, float deltaTime) {}
};
```

`SceneManager::updateActive()` is called before system phases each frame, not as a system itself.

Currently only one concrete `Scene` implementation exists: `GenericFileScene` (`src/game/scenes/GenericFileScene.h`) which loads any `.scene` file by path plus optional scripted geometry hooks.

**Level loading data flow:**

```
.scene file
    ↓ LevelLoader::load()
LevelDef struct (meshes[], lights[], colliders[], playerSpawn, archetypes[])
    ↓ LevelBuilder
    ECS entities spawned: TransformComponent + MeshComponent + LightComponent + etc.
    ↓ GameplayPrefabs::spawnDoubleDoor / spawnCheckpoint
    Compound entities (door = DoorComponent + 2x DoorLeafComponent + InteractableComponent)
    ↓
Systems operate on components each frame
```

`LevelDef` uses `nodeId` / `parentNodeId` string pairs to represent the scene hierarchy (for round-trip with editor). The hierarchy is resolved via `resolveLevelHierarchy()` before spawning.

---

## Editor Integration

The editor is an independent application (`apps/level_editor/`) that shares the game and engine libraries.

**EditorSceneDocument** — the editor's authoritative scene state:
- Stores a flat `std::vector<EditorSceneObject>` where each object wraps a `std::variant` of `LevelMeshPlacement | LevelLightPlacement | LevelBoxColliderPlacement | ...`
- Maintains parent/child relationships via `nodeId`/`parentNodeId` strings (same as `LevelDef`)
- `toLevelDef()` converts to `LevelDef` for serialization or preview rebuild
- Tracks two dirty flags independently: `sceneDirty_` and `environmentDirty_`
- Exposes `sceneRevision_` and `environmentRevision_` counters for change detection

**EditorPreviewWorld** — a live ECS world for the static editor view:
- Rebuilt from `EditorSceneDocument` on scene changes
- Maintains an `ownerMap` (entity → document object ID) for click-to-select
- Uses `MaterialTextureLibrary` for material resolution, same as runtime

**EditorRuntimePreviewSession** — wraps `RuntimeGameSession` for in-editor play mode:
- Converts `EditorSceneDocument` → `LevelDef` → rebuilds full runtime session
- Intercepts GLFW input and injects it via `InputSystem::setKeyPressed()` / `setMouseDelta()` etc. (bypassing GLFW callback path)
- `captured_` flag controls whether mouse input goes to editor camera or runtime player

**Undo/Redo** — `EditorCommandStack` / `EditorDocumentStateCommand`:
- Full document state snapshot strategy (not fine-grained command diffs)
- `EditorSceneDocumentState` is a plain copyable struct with `objects`, `environment`, `nextObjectId`, `dirty` flags
- Ring buffer limited to 256 commands
- Compares serialized scene/environment content to detect save state

Unlike **Godot's** undo/redo (which uses method call pairs), this engine uses a memento pattern with full state captures. This is simpler to implement but has higher memory cost per command.

---

## Error Handling

**Strategy:** Assertions and exceptions for programmer errors; log-and-continue for asset load failures.

**Patterns:**
- `Application::getService<T>()` throws `std::runtime_error` if service not registered
- Asset loaders return empty/default structs on failure and log via spdlog
- Physics system uses Jolt's internal error callbacks (not exceptions)
- No RTTI except `std::type_index` in service locator and event bus

---

## Cross-Cutting Concerns

**Logging:** spdlog with category-less global logger. `spdlog::info/warn/error` throughout. Editor has a custom `EditorConsoleSink` (`src/engine/core/EditorConsoleSink.h`) that captures log output for the in-editor console panel.

**Validation:** `ContentRegistry::validateMaterialInheritance()` run at load time. Material inheritance is resolved at use time via `resolveMaterialDefinition()` which walks `parent` chains.

**Authentication:** Not applicable.

**Hot reload:** `ContentRegistry::pollMaterialHotReload()` — called per-frame in editor, polls `.material` file modification times at 500ms intervals and triggers `RuntimeSceneRenderer::reloadContent()`.

---

*Architecture analysis: 2026-04-01*
