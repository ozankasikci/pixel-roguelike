# Codebase Structure

**Analysis Date:** 2026-04-01

## Directory Layout

```
gsd-3d-roguelike/
├── apps/                   # Executable entry points (one per binary)
│   ├── runtime/            # pixel-roguelike — the playable game
│   ├── level_editor/       # level-editor — the scene authoring tool
│   └── model_viewer/       # procedural-model-viewer — standalone mesh preview
├── assets/                 # All runtime assets (shaders, meshes, scenes, defs)
│   ├── defs/               # Content definition files (.weapon, .environment, etc.)
│   ├── environments/       # (reserved; .environment files live in defs/environments/)
│   ├── fonts/              # TTF fonts (editor only)
│   ├── materials/          # .material definition files
│   ├── meshes/             # 3D mesh files (.glb, .fbx)
│   ├── packs/              # Third-party asset packs (raw, not yet integrated)
│   ├── prefabs/            # .prefab files for gameplay archetypes
│   ├── scenes/             # .scene files (authored in level editor)
│   ├── shaders/
│   │   ├── engine/         # Post-process shaders (composite, stylize, bloom, ssao, shadow)
│   │   └── game/           # Scene PBR shaders (scene.vert / scene.frag)
│   ├── skies/              # Cubemap PNGs and TGA sky textures
│   └── textures/           # PBR texture sets (albedo, normal, roughness, ao)
├── cmake/                  # Custom CMake helpers (DesktopApp.cmake)
├── external/               # Vendored headers (ImGuizmo, stb_image_write, stb_vorbis)
├── src/
│   ├── engine/             # Engine layer — no game/editor dependencies
│   ├── game/               # Game layer — depends on engine only
│   └── editor/             # Editor layer — depends on game and engine
├── tests/
│   ├── common/             # TestSupport.h shared test helpers
│   ├── engine/             # Engine-layer unit tests
│   └── game/               # Game-layer integration tests
├── tools/                  # Offline tools (procedural mesh generators)
├── CMakeLists.txt          # Root build configuration with FetchContent deps
├── CLAUDE.md               # Project conventions and architecture reference
└── .planning/              # GSD workflow artifacts (phases, codebase docs)
```

## Source Tree — Engine Layer (`src/engine/`)

```
src/engine/
├── audio/
│   └── AudioSystem.h/.cpp          # OpenAL Soft 3D audio, System subclass
├── core/
│   ├── Application.h/.cpp          # Main loop, system registry, service locator
│   ├── EventBus.h                  # Type-safe pub/sub (std::type_index keyed)
│   ├── System.h                    # Base class: init/update/shutdown interface
│   ├── Time.h/.cpp                 # Frame delta time tracking
│   ├── Window.h/.cpp               # GLFW window + context creation
│   ├── PathUtils.h/.cpp            # resolveProjectPath() utility
│   ├── ProjectConfig.h/.cpp        # assets/project.cfg (last-opened scene)
│   ├── MathUtils.h                 # GLM helper functions
│   └── EditorConsoleSink.h         # spdlog sink for in-editor log panel
├── ecs/
│   └── Registry.h                  # EnTT include alias (registry lives in Application)
├── input/
│   ├── InputSystem.h/.cpp          # GLFW callbacks, action map, cursor lock
│   └── ActionMap.h                 # Named action bindings
├── physics/
│   └── PhysicsSystem.h/.cpp        # Jolt Physics behind pimpl (character controller)
├── rendering/
│   ├── SceneRenderPipeline.h/.cpp  # Orchestrates full GPU pipeline
│   ├── assets/
│   │   ├── AssetCache.h            # Generic key→resource cache
│   │   ├── AssimpLoader.h/.cpp     # FBX/multi-format mesh import
│   │   ├── GltfLoader.h/.cpp       # tinygltf glTF 2.0 loader
│   │   ├── ModelLoader.h/.cpp      # Unified loader dispatching to Assimp/tinygltf
│   │   ├── Texture2D.h/.cpp        # RAII OpenGL 2D texture (stb_image backed)
│   │   └── TextureCube.h/.cpp      # RAII OpenGL cubemap texture
│   ├── core/
│   │   ├── Framebuffer.h/.cpp      # RAII FBO with color + depth attachments
│   │   └── Shader.h/.cpp           # RAII GLSL program, uniform setters
│   ├── geometry/
│   │   ├── Mesh.h/.cpp             # RAII VAO/VBO/EBO
│   │   ├── MeshGeometry.h          # RawMeshData struct (CPU-side vertices/indices)
│   │   ├── MeshLibrary.h/.cpp      # Named mesh registry (string → Mesh*)
│   │   └── Renderer.h/.cpp         # drawScene() — sets uniforms, binds textures, draws
│   ├── lighting/
│   │   ├── RenderLight.h           # LightType enum, RenderLight struct, LightingEnvironment
│   │   ├── ShadowMap.h/.cpp        # Single spot-light shadow map FBO
│   │   ├── CascadedShadowMap.h/.cpp # CSM for directional sun light
│   │   └── LtcData.h/.cpp          # LTC lookup tables for area lights
│   └── post/
│       ├── BloomPass.h/.cpp        # Dual-pass bloom (downsample → upsample)
│       ├── CompositePass.h/.cpp    # Tone map + fog + sky + SSAO blend
│       ├── SsaoPass.h/.cpp         # Screen-space ambient occlusion
│       ├── StylizePass.h/.cpp      # Edge detection, vignette, grain, scanlines
│       ├── PostProcessParams.h     # 30+ configurable post-process parameters
│       ├── SkySettings.h           # Sky cubemap + horizon configuration
│       └── SkyTextureLibrary.h/.cpp # Loads and caches sky cubemap textures
├── scene/
│   ├── Scene.h                     # Abstract scene interface: onEnter/onExit/onUpdate
│   └── SceneManager.h/.cpp         # Push/pop scene stack, calls updateActive() per frame
└── ui/
    ├── ImGuiLayer.h/.cpp           # Dear ImGui init/beginFrame/endFrame/shutdown
    └── Screenshot.h/.cpp           # PNG screenshot capture to disk
```

## Source Tree — Game Layer (`src/game/`)

```
src/game/
├── components/                     # All ECS component structs (POD, no methods)
│   ├── TransformComponent.h        # position/rotation/scale + modelMatrix()
│   ├── MeshComponent.h             # Mesh* + materialId + tint
│   ├── LightComponent.h            # Light type and params
│   ├── CameraComponent.h           # FOV, near/far planes
│   ├── CharacterControllerComponent.h
│   ├── StaticColliderComponent.h
│   ├── InteractableComponent.h     # Prompt text, interact distance/dot threshold
│   ├── DoorComponent.h             # leftLeaf/rightLeaf entities, open progress
│   ├── DoorLeafComponent.h         # Per-leaf open angle animation state
│   ├── CheckpointComponent.h
│   ├── PlayerMovementComponent.h
│   ├── ViewmodelComponent.h        # First-person weapon mesh offset/sway state
│   ├── AudioSourceComponent.h
│   ├── PlayerSpawnComponent.h
│   ├── PlayerInteractionLockComponent.h
│   ├── PlayerTag.h                 # Zero-size marker
│   ├── PrimaryCameraTag.h          # Zero-size marker
│   ├── ControllableTag.h           # Zero-size marker
│   └── AudioListenerTag.h          # Zero-size marker
├── content/
│   ├── ContentRegistry.h/.cpp      # Loads and owns all definition assets
│   └── ParseUtils.h                # Text parsing helpers for .scene/.def files
├── level/
│   ├── LevelDef.h                  # Data structs: LevelMeshPlacement, LevelDef, etc.
│   ├── LevelBuildContext.h         # registry + meshLibrary + entities refs bundle
│   ├── LevelBuilder.h/.cpp         # Entity factory: addMesh, addLight, addCollider
│   └── LevelLoader.h/.cpp          # .scene file → LevelDef → calls LevelBuilder
├── levels/
│   ├── GameAssets.h/.cpp           # registerAllGameAssets() — registers all .glb/.fbx meshes
│   ├── cathedral/
│   │   └── CathedralPrefabs.h/.cpp # Cathedral-specific procedural geometry helpers
│   └── prison/                     # (empty — institutional room geometry in GenericFileScene)
├── prefabs/
│   ├── GameplayPrefabData.h        # CheckpointSpawnSpec, DoubleDoorSpawnSpec structs
│   └── GameplayPrefabs.h/.cpp      # spawnCheckpoint(), spawnDoubleDoor(), spawnGameplayPrefab()
├── rendering/
│   ├── RuntimeSceneRenderer.h/.cpp # ECS query → RenderObject/RenderLight → SceneRenderPipeline
│   ├── MaterialDefinition.h/.cpp   # MaterialDefinition struct, resolve/serialize/load
│   ├── MaterialTextureLibrary.h/.cpp # Resolves materialId → bound GL textures
│   ├── EnvironmentDefinition.h/.cpp # Wraps PostProcessParams + SkySettings + LightingEnvironment
│   ├── EnvironmentProfile.h        # Enum for preset environments (Default, Outdoor, etc.)
│   ├── EnvironmentDebugSync.h/.cpp # ImGui debug panel for live environment tweaking
│   ├── MeshAssetProvider.h         # Registry context singleton for mesh access in systems
│   ├── RuntimeCameraMath.h         # View/projection matrix helpers
│   └── RetroPalette.h              # (legacy — palette colors, not actively used)
├── runtime/
│   ├── RuntimeGameSession.h/.cpp   # Self-contained play session: registry + physics + renderer
│   └── RuntimeGameplay.h/.cpp      # Free-function gameplay logic (interaction, doors, movement, camera)
├── scenes/
│   └── GenericFileScene.h/.cpp     # Scene impl that loads any .scene file + optional scripted geometry
├── session/
│   ├── RunSession.h                # Persistent per-run state (inventory, equipped weapons, checkpoints)
│   └── EquipmentState.h/.cpp       # Equipment weight/slot validation helpers
├── systems/
│   ├── PlayerMovementSystem.h/.cpp # Reads InputSystem → drives PhysicsSystem character controller
│   ├── CameraSystem.h/.cpp         # Mouse delta → camera yaw/pitch on PrimaryCameraTag entity
│   ├── RenderSystem.h/.cpp         # Owns RuntimeSceneRenderer, drives full render each frame
│   ├── DoorSystem.h/.cpp           # Door open/close animation ticks
│   ├── InteractionSystem.h/.cpp    # Raycasts for interactable prompt display
│   ├── CheckpointSystem.h/.cpp     # Checkpoint trigger and respawn logic
│   ├── InventorySystem.h/.cpp      # Inventory menu input and equip/unequip
│   ├── AudioListenerSystem.h/.cpp  # Syncs PrimaryCameraTag position to OpenAL listener
│   └── AudioListenerSystem.h/.cpp
└── ui/
    ├── GameOverlays.h/.cpp         # Interaction prompts, inventory HUD (ImGui rendered)
    ├── InteractionPromptState.h    # Prompt visibility/text state
    ├── InteractionFocusState.h     # Which entity is in focus for interaction
    └── InventoryMenuState.h        # Inventory open/close + selection state
```

## Source Tree — Editor Layer (`src/editor/`)

```
src/editor/
├── assets/
│   └── EditorAssetBrowser.h/.cpp   # Asset browser panel (meshes, materials, archetypes)
├── build/
│   └── EditorBuildSystem.h/.cpp    # Export/build pipeline (scene validation, asset packaging)
├── core/
│   ├── EditorCommand.h/.cpp        # IEditorCommand + EditorCommandStack (undo/redo)
│   ├── EditorLayoutPreset.h/.cpp   # Save/load ImGui dock layout presets to disk
│   ├── EditorRuntimePreviewSession.h/.cpp # Wraps RuntimeGameSession for in-editor play
│   └── LevelEditorCore.h/.cpp      # Top-level editor helpers (scene load, sorted asset lists)
├── render/
│   ├── EditorScenePreviewRenderer.h/.cpp  # collectRenderObjects + overlays for editor view
│   ├── EditorAssetPreviewRenderer.h/.cpp  # Renders single mesh for asset browser thumbnails
│   └── EditorViewportRenderer.h/.cpp      # Combines scene + gizmos + selection into viewport FBO
├── scene/
│   ├── EditorSceneDocument.h/.cpp  # Authoritative scene state: objects[], environment, dirty flags
│   ├── EditorSceneSerializer.h/.cpp # Document ↔ LevelDef ↔ .scene file serialization
│   ├── EditorPreviewWorld.h/.cpp   # ECS world built from document for static editor preview
│   └── EditorSelectionSystem.h/.cpp # Click/box selection, hover highlight
├── ui/
│   ├── LevelEditorUi.h/.cpp        # Top-level ImGui UI state (EditorUiState struct)
│   ├── EditorPanels.h/.cpp         # Inspector, environment, material editor panels
│   └── EditorOutlinerPanel.h/.cpp  # Scene hierarchy tree panel
└── viewport/
    ├── EditorViewportController.h/.cpp  # Camera orbit, pan, zoom; ImGuizmo gizmo integration
    └── EditorViewportInteraction.h/.cpp # Mouse picking, placement mode, drag state
```

## App Entry Points

**`apps/runtime/main.cpp`** — `pixel-roguelike` binary:
- Creates `Application`, registers all systems by phase, loads `ContentRegistry`
- Scene resolution order: `--scene` arg → `assets/project.cfg` last scene → ImGui scene picker
- Pushes `GenericFileScene` onto `SceneManager`, calls `app.run()`

**`apps/level_editor/main.cpp`** — `level-editor` binary (99KB):
- Creates `Application` with editor-specific system set
- Owns `EditorSceneDocument`, `EditorCommandStack`, `EditorPreviewWorld`, `EditorRuntimePreviewSession`
- Dock-based ImGui layout with viewport, outliner, inspector, asset browser, environment, console panels

**`apps/model_viewer/main.cpp`** — `procedural-model-viewer` binary:
- Lightweight standalone viewer using `game_rendering` library
- No ECS systems — directly calls `SceneRenderPipeline` for mesh inspection

## Key File Locations

**Entry points:**
- `apps/runtime/main.cpp` — game boot, system registration, scene selection
- `apps/level_editor/main.cpp` — editor boot, UI wiring
- `apps/model_viewer/main.cpp` — model viewer boot

**Core engine files:**
- `src/engine/core/Application.h` — main loop, phase system, service locator API
- `src/engine/rendering/SceneRenderPipeline.h` — full GPU pipeline orchestration
- `src/engine/core/System.h` — base class for all systems

**Core game files:**
- `src/game/runtime/RuntimeGameSession.h` — isolated play session (used by editor preview)
- `src/game/rendering/RuntimeSceneRenderer.h` — ECS → renderer bridge
- `src/game/level/LevelDef.h` — scene data model (source of truth for .scene files)
- `src/game/content/ContentRegistry.h` — all loaded definition assets

**Asset serialization:**
- `src/game/level/LevelDef.h` — `loadLevelDef()`, `serializeLevelDef()`, `saveLevelDef()`
- `src/game/rendering/MaterialDefinition.h` — `.material` file round-trip
- `src/game/rendering/EnvironmentDefinition.h` — `.environment` file round-trip
- `src/game/content/ContentRegistry.h` — loads `.weapon`, `.enemy`, `.item`, `.skill`, `.prefab` files

**Shaders:**
- `assets/shaders/game/scene.frag` — PBR scene shader (42KB, all lighting + procedural textures)
- `assets/shaders/engine/composite.frag` — tone map, fog, sky, SSAO composite (12KB)
- `assets/shaders/engine/stylize.frag` — edge detection, vignette, film grain

## Asset File Formats

| Extension | Purpose | Location |
|-----------|---------|----------|
| `.scene` | Level scene data (text, JSON-like) | `assets/scenes/` |
| `.material` | Material definition (inheritable) | `assets/materials/` |
| `.environment` | Post-process + sky + lighting config | `assets/defs/environments/` |
| `.prefab` | Gameplay archetype definition | `assets/prefabs/gameplay/` |
| `.weapon` | Weapon definition | `assets/defs/weapons/` |
| `.enemy` | Enemy definition | `assets/defs/enemies/` |
| `.item` | Item definition | `assets/defs/items/` |
| `.skill` | Skill definition | `assets/defs/skills/` |
| `.glb` | glTF 2.0 mesh (preferred for new meshes) | `assets/meshes/` |
| `.fbx` | FBX mesh (legacy, loaded via Assimp) | `assets/meshes/` |

## Naming Conventions

**Files:**
- `PascalCase.h` / `PascalCase.cpp` — matches primary class name
- Exception: `main.cpp` for entry points
- Test files: `test_snake_case.cpp`

**Directories:**
- `snake_case/` for all source and asset directories
- Exception: `ImGuizmo/` (vendored, kept as-is)

**Classes/Structs:** `PascalCase` — `RuntimeGameSession`, `TransformComponent`, `SceneRenderPipeline`

**Components:** suffix `Component` for data components, suffix `Tag` for marker types — `MeshComponent`, `PlayerTag`

**Systems:** suffix `System` — `PlayerMovementSystem`, `RenderSystem`

**Editors types:** prefix `Editor` — `EditorSceneDocument`, `EditorCommandStack`

## CMake Targets

```
pixel-roguelike       → gameplay → game_rendering → game_content → engine_rendering → engine_core
level-editor          → editor   → gameplay                        engine_ui
procedural-model-viewer → game_rendering                           engine_scene
                                                                   engine_input
                                                                   engine_physics
```

CMake targets map to `add_subdirectory()` calls in `src/engine/`, `src/game/`, `src/editor/`. Custom macro `pixel_roguelike_add_test()` defined in `cmake/DesktopApp.cmake` registers test executables.

## Where to Add New Code

**New ECS component:**
- Header: `src/game/components/YourComponent.h`
- Follow POD struct pattern, no methods (except pure computed helpers like `modelMatrix()`)
- No registration needed — EnTT discovers component types at compile time

**New system:**
- Header + cpp: `src/game/systems/YourSystem.h/.cpp`
- Inherit `System`, implement `init/update/shutdown`
- Register in `apps/runtime/main.cpp` with appropriate `UpdatePhase`

**New level scene:**
- Author in level editor → saves to `assets/scenes/your_level.scene`
- No code changes needed — `GenericFileScene` loads any `.scene` file

**New material:**
- Create `assets/materials/your_material.material`
- Can declare `parent: existing_material` to inherit properties
- Hot-reloads in editor automatically (500ms poll)

**New environment preset:**
- Create `assets/defs/environments/your_env.environment`
- Loaded by `ContentRegistry::loadDefaults()`

**New gameplay archetype (door variant, checkpoint variant):**
- Create `assets/prefabs/gameplay/your_prefab.prefab`
- Register archetype kind in `ContentRegistry.h` if it's a new `GameplayArchetypeKind`
- Implement spawn function in `src/game/prefabs/GameplayPrefabs.h/.cpp`

**New mesh:**
- Place `.glb` in `assets/meshes/`
- Register in `src/game/levels/GameAssets.cpp` inside `registerAllGameAssets()`

**New engine post-process pass:**
- Shader: `assets/shaders/engine/your_pass.vert/.frag`
- Pass class: `src/engine/rendering/post/YourPass.h/.cpp`
- Wire into `SceneRenderPipeline` at `src/engine/rendering/SceneRenderPipeline.h/.cpp`
- Add parameters to `PostProcessParams` if user-configurable

**New test:**
- `tests/game/test_your_feature.cpp` (for game layer) or `tests/engine/test_your_feature.cpp`
- Register in `tests/game/CMakeLists.txt` using `pixel_roguelike_add_test()`

## Special Directories

**`.planning/`:**
- Purpose: GSD workflow artifacts — phases, quick tasks, debug investigations, codebase docs
- Generated: Partially (by GSD commands)
- Committed: Yes

**`external/`:**
- Purpose: Vendored single-header libraries (ImGuizmo, stb_image_write, stb_vorbis)
- Generated: No (manually copied)
- Committed: Yes

**`assets/packs/`:**
- Purpose: Raw third-party asset packs not yet integrated into the project asset pipeline
- Generated: No
- Committed: Yes (binary assets)

**`editor_layouts/`:**
- Purpose: Saved ImGui dock layout presets (created by `saveLayoutPresetFromUi()`)
- Generated: Yes (at runtime by editor)
- Committed: No (user-specific)

---

*Structure analysis: 2026-04-01*
