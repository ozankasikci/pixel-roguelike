# Codebase Structure

**Analysis Date:** 2026-04-05

## Directory Layout

```
pixel-roguelike/
├── src/
│   ├── engine/              # Cross-platform subsystems (no game dependencies)
│   │   ├── core/            # Application, Window, EventBus, System base, Time
│   │   ├── ecs/             # EnTT convenience header
│   │   ├── rendering/       # OpenGL pipeline, shaders, meshes, lighting, post-process
│   │   │   ├── core/
│   │   │   ├── geometry/
│   │   │   ├── assets/
│   │   │   ├── lighting/
│   │   │   └── post/
│   │   ├── input/           # GLFW input aggregation, input state
│   │   ├── physics/         # Jolt Physics wrapper
│   │   ├── audio/           # OpenAL 3D audio
│   │   ├── scene/           # SceneManager base
│   │   └── ui/              # ImGui layer, screenshots
│   │
│   ├── game/                # Roguelike gameplay logic and content
│   │   ├── components/      # ECS component POD structs
│   │   ├── systems/         # Game-layer systems (PlayerMovement, Camera, Render, etc.)
│   │   ├── behavior/        # Behavior tree and action execution (doors, messages, etc.)
│   │   ├── content/         # ContentRegistry, asset definition loaders
│   │   ├── rendering/       # RuntimeSceneRenderer, MaterialDefinition, EnvironmentDefinition
│   │   ├── level/           # LevelDef (serializable scene), LevelBuilder, LevelLoader
│   │   ├── levels/          # Level-specific code (cathedral/, prison/)
│   │   │   ├── cathedral/
│   │   │   └── prison/
│   │   ├── runtime/         # RuntimeGameSession, RuntimeGameplay (core play loop)
│   │   ├── session/         # RunSession (persistent play state), EquipmentState
│   │   ├── prefabs/         # GameplayPrefabs (composite entity factories)
│   │   ├── scenes/          # Scene-specific implementations (GenericFileScene)
│   │   └── ui/              # Game HUD overlays (GameOverlays)
│   │
│   └── editor/              # Level editor (only runs in editor, not in shipped game)
│       ├── core/            # LevelEditorCore, layout management, command stack
│       ├── scene/           # EditorSceneDocument, serialization, preview world
│       ├── viewport/        # Camera control, ImGuizmo integration
│       ├── ui/              # Inspector, outliner, asset browser, environment panel
│       ├── render/          # Preview renderers (EditorScenePreviewRenderer)
│       ├── assets/          # Asset browser backend
│       ├── build/           # Editor build context
│       └── debug/           # Unix socket debug harness
│
├── apps/
│   ├── runtime/             # Main game executable (main.cpp)
│   ├── level_editor/        # Level editor executable (main.cpp)
│   └── model_viewer/        # Procedural model preview tool
│
├── tests/
│   ├── cmake/               # CMake build tests
│   ├── common/              # Shared test utilities
│   ├── engine/              # Engine subsystem tests
│   ├── game/                # Game logic tests
│   ├── editor/              # Editor tests
│   └── data/                # Test fixtures (models, scenes)
│
├── assets/
│   ├── scenes/              # .scene files (JSON level definitions)
│   ├── materials/           # .material files (JSON with shader parameters)
│   ├── environments/        # .environment files (sky, lighting presets)
│   ├── prefabs/             # .prefab files (reusable entity templates)
│   ├── meshes/              # .glb/.fbx model files
│   ├── textures/            # PNG/TGA albedo, normal, roughness maps
│   ├── shaders/             # GLSL 4.10 vertex/fragment shaders
│   │   ├── engine/          # Post-process, shadow depth, composite shaders
│   │   └── game/            # Scene rendering (PBR, procedural textures)
│   └── skies/               # Cubemap skyboxes + horizon overlays
│
├── cmake/
│   └── DesktopApp.cmake     # CMake utilities for FetchContent targets
│
├── external/
│   └── ImGuizmo/            # Gizmo library for editor
│
├── editor_layouts/          # ImGui dock layout presets (generated)
│
├── docs/
│   ├── concept-art/         # Level design mockups (per-level subdirs)
│   └── plans/               # Planning documents
│
├── .planning/               # GSD workflow artifacts
│   ├── phases/              # Work phase documents
│   └── codebase/            # ARCHITECTURE.md, STRUCTURE.md, etc.
│
├── CMakeLists.txt           # Root build configuration
├── CLAUDE.md                # Project conventions and stack (checked into repo)
└── .clang-format            # Code style: LLVM 4-space indent, 100-char line
```

## Directory Purposes

**`src/engine/`:**
- Purpose: Reusable subsystems for any C++ graphics engine
- Contains: Graphics (OpenGL), input (GLFW), physics (Jolt), audio (OpenAL), UI (ImGui)
- Key files: `core/Application.h`, `rendering/SceneRenderPipeline.h`, `physics/PhysicsSystem.h`
- Constraint: Zero dependencies on `game/` or `editor/`

**`src/engine/core/`:**
- `Application.h` — Main loop, phase-ordered system execution, service locator
- `Window.h` — GLFW window wrapper
- `System.h` — Base class for phased systems
- `EventBus.h` — Type-safe pub/sub
- `Time.h` — Frame timing

**`src/engine/rendering/`:**
- **core/**: Shader compilation, Framebuffer (FBO) management
- **geometry/**: Mesh (VAO/VBO), MeshLibrary registry, Renderer draw dispatch
- **assets/**: Model loaders (Assimp, tinygltf), texture loading (stb_image)
- **lighting/**: RenderLight struct, shadow maps, reflection probes
- **post/**: Post-process passes (bloom, SSAO, stylize, composite)

**`src/game/`:**
- Purpose: Roguelike-specific content, gameplay logic, and systems
- Three sub-libraries: `game_content` (definitions) → `game_rendering` (visualization) → `gameplay` (runtime systems)
- Key files: `runtime/RuntimeGameSession.h` (self-contained simulation), `level/LevelDef.h` (serializable scene), `components/*.h` (ECS data)

**`src/game/components/`:**
- All files are POD struct definitions (no methods, no inheritance)
- Examples: `TransformComponent.h` (position/rotation/scale), `MeshComponent.h` (mesh reference), `PlayerTag.h` (marker)
- Naming: PascalCase struct names matching component type

**`src/game/systems/`:**
- All inherit from `System` base class
- Examples: `PlayerMovementSystem.h`, `CameraSystem.h`, `RenderSystem.h`
- Pattern: Constructor takes dependencies (InputSystem, PhysicsSystem); update() queries ECS and modifies components

**`src/game/rendering/`:**
- `RuntimeSceneRenderer.h` — Queries ECS, collects RenderObject/RenderLight, dispatches to SceneRenderPipeline
- `MaterialDefinition.h` — Shader parameter templates with inheritance
- `EnvironmentDefinition.h` — Sky/lighting/post-process presets
- `MaterialTextureLibrary.h` — GPU texture binding pool

**`src/game/level/`:**
- `LevelDef.h` — Serializable scene structure (placements of meshes, lights, colliders, archetypes)
- `LevelLoader.h` — File I/O and ECS instantiation
- `LevelBuilder.h` — Factory for spawning entities from placements
- `LevelBuildContext.h` — Shared state during level construction

**`src/game/runtime/`:**
- `RuntimeGameSession.h` — **Central**: Self-contained simulation (registry, physics, input, renderer). Used by both runtime and editor.
- `RuntimeGameplay.h` — Free functions for gameplay feature updates (not a class)
- Lifetime: `build()` → `resetForPlay()` → `tick()` → `render()` → `clear()`

**`src/game/content/`:**
- `ContentRegistry.h` — Asset resolver for meshes, materials, environments, weapons, enemies, archetypes
- Supports hot-reload for editor iteration
- Validation of material inheritance chains

**`src/game/behavior/`:**
- `ActionTypes.h` — Variant-based action system (OpenDoor, PlaySound, ShowMessage, etc.)
- `BehaviorSystem.h` — Executes queued actions
- `DoorAnimationSystem.h` — Animates door rotation

**`src/editor/`:**
- Purpose: Scene authoring tool (does not ship with game)
- Consumes game layer directly for preview (`RuntimeGameSession`) and serialization (`LevelDef`)

**`src/editor/scene/`:**
- `EditorSceneDocument.h` — In-memory scene with parent/child hierarchy, world transforms
- `EditorSceneObject` — Variant holding mesh/light/collider placements
- Serialization to/from `LevelDef` and `.scene` JSON files

**`src/editor/core/`:**
- `LevelEditorCore.h` — Dock layout, scene loading, layout presets
- `EditorRuntimePreviewSession.h` — Wraps `RuntimeGameSession` for live preview
- `EditorCommandStack.h` — Undo/redo

**`src/editor/viewport/`:**
- Camera orbit control
- ImGuizmo integration for translate/rotate/scale manipulation

**`src/editor/debug/`:**
- Unix socket remote control server (`/tmp/pixel-roguelike-editor-{pid}.sock`)
- 33 commands for inspection and mutation (described in project MEMORY.md)

**`assets/scenes/`:**
- `.scene` files — JSON serialized `LevelDef` structs
- Format: Placements of meshes, lights, colliders, archetypes with hierarchy and properties

**`assets/materials/`:**
- `.material` files — JSON `MaterialDefinition` structs
- Properties: Shader parameters (specular, roughness, normal strength), texture references, material kind
- Inheritance: Child materials can override parent properties

**`assets/shaders/`:**
- **engine/**: Post-process (composite, stylize, bloom, SSAO), shadow depth
- **game/**: Main scene shader (PBR-like, 32 max lights, shadow maps, procedural textures)
- All GLSL 4.10 (#version 410 core — macOS ceiling)

**`tests/`:**
- Standalone executables (no external test framework)
- Exit code 0 = pass, non-zero = fail
- Custom CMake macro: `pixel_roguelike_add_test(target_name source.cpp)`
- Examples: shader compilation tests, material parsing tests, level loading tests

## Key File Locations

**Entry Points:**
- `apps/runtime/main.cpp` — Main game executable
- `apps/level_editor/main.cpp` — Level editor executable
- `apps/model_viewer/main.cpp` — Procedural model viewer

**Core Application:**
- `src/engine/core/Application.h` — Main loop, system registration, service locator
- `src/engine/core/System.h` — Base class for all systems
- `src/engine/core/EventBus.h` — Type-safe pub/sub

**ECS & Components:**
- `src/engine/ecs/Registry.h` — EnTT convenience header
- `src/game/components/*.h` — POD component definitions (TransformComponent, MeshComponent, etc.)

**Rendering Pipeline:**
- `src/engine/rendering/SceneRenderPipeline.h` — Coordinates shadow/scene/post-process passes
- `src/engine/rendering/geometry/Renderer.h` — Draw dispatch to GPU
- `src/game/rendering/RuntimeSceneRenderer.h` — Queries ECS, collects render data

**Game Content:**
- `src/game/content/ContentRegistry.h` — Asset definitions and resolution
- `src/game/level/LevelDef.h` — Serializable scene structure
- `src/game/rendering/MaterialDefinition.h` — Shader parameter templates

**Level Loading:**
- `src/game/level/LevelLoader.h` — Deserialize .scene files and instantiate ECS entities
- `src/game/level/LevelBuilder.h` — Entity factory

**Game Systems:**
- `src/game/systems/PlayerMovementSystem.h`
- `src/game/systems/CameraSystem.h`
- `src/game/systems/RenderSystem.h`
- `src/game/systems/InteractionSystem.h`
- `src/game/systems/CheckpointSystem.h`
- `src/game/systems/InventorySystem.h`

**Runtime Simulation:**
- `src/game/runtime/RuntimeGameSession.h` — Self-contained play session container
- `src/game/runtime/RuntimeGameplay.h` — Gameplay feature updates (free functions)

**Editor:**
- `src/editor/scene/EditorSceneDocument.h` — Scene graph with serialization
- `src/editor/core/LevelEditorCore.h` — Editor orchestration
- `src/editor/ui/LevelEditorUi.h` — ImGui windows (outliner, inspector, asset browser)

**Configuration:**
- `CMakeLists.txt` — Root CMake build
- `src/engine/CMakeLists.txt` — Engine library targets
- `src/game/CMakeLists.txt` — Game library targets
- `src/editor/CMakeLists.txt` — Editor library targets
- `.clang-format` — Code style (LLVM, 4-space, 100-char)

## Naming Conventions

**Files:**
- Classes/components: `PascalCase.h` matching primary class (e.g., `TransformComponent.h`)
- Free functions: `snake_case.h` (e.g., `level_def.h` hypothetically, though most are in .cpp)
- Units: UPPERCASE prefix convention sometimes used (e.g., `Time.h`, not `TimeUtils.h`)

**Directories:**
- Lowercase with underscores: `engine/`, `game/`, `editor/`, `src/`
- Functional grouping: `components/`, `systems/`, `rendering/`, `physics/`, `audio/`, `level/`

**Code Elements:**
- Classes/Structs: `PascalCase` (TransformComponent, RuntimeGameSession, PhysicsSystem)
- Functions/Methods: `camelCase` (update(), getCharacterPosition(), loadFromFile())
- Variables: `snake_case` generally; private members use trailing underscore (`registry_`, `window_`)
- Constants: `k` prefix + PascalCase (kMaxRenderLights, kMaterialKindCount)
- Enums: `PascalCase` enum class with `PascalCase` values (UpdatePhase::Input, ColliderShape::Box)

## Where to Add New Code

**New Gameplay Feature (Door System, Checkpoint, etc.):**
- Component definitions: `src/game/components/{FeatureName}Component.h`
- System class: `src/game/systems/{FeatureName}System.h/.cpp`
- Register in main game loop: `apps/runtime/main.cpp` (add to Application phase)
- Tests: `tests/game/{feature_test}.cpp`

**New Rendering Technique (Post-Process, Lighting):**
- Shader: `assets/shaders/engine/{technique_name}.frag` (+ .vert if needed)
- C++ wrapper: `src/engine/rendering/post/{TechniqueName}Pass.h/.cpp` (or `lighting/` as appropriate)
- Integration: Call from `SceneRenderPipeline::render()` or `RuntimeSceneRenderer`
- Material updates: `src/game/rendering/MaterialDefinition.h` if new material property needed

**New Editor Panel (Behavior Editor, Script Editor):**
- ImGui code: `src/editor/ui/LevelEditorUi.h/.cpp` (add new window function)
- Data struct: Define in `EditorSceneDocument` or separate `EditorFoo.h`
- Layout integration: Update `EditorLayoutPreset` and dock layout builder
- Serialization: Update `EditorSceneSerializer` if needed

**New Asset Type (Weapon, Enemy, Skill):**
- Definition struct: `src/game/content/` (e.g., `WeaponDefinition` already exists in `ContentRegistry.h`)
- Loader function: `src/game/content/{AssetType}Loader.h`
- Registry method: Add to `ContentRegistry::load{AssetType}()` and cache
- File format: Choose JSON or binary; document in comments

**New Engine Subsystem (Networking, Analytics):**
- Header: `src/engine/{subsystem}/{Subsystem}.h`
- Register as service: In Application setup, call `app.emplaceService<NewService>()`
- Access from game: `app.getService<NewService>()`

## Special Directories

**`assets/scenes/`:**
- Purpose: Level definition files in JSON format
- Generated: No (authored by editor)
- Committed: Yes
- Format: `.scene` files are serialized `LevelDef` structs (meshes, lights, colliders, archetypes, hierarchy)

**`assets/materials/`:**
- Purpose: Shader parameter templates with inheritance
- Generated: No (authored in editor or written by hand)
- Committed: Yes
- Format: `.material` files are JSON serialized `MaterialDefinition` structs

**`assets/environments/`:**
- Purpose: Sky/lighting/post-process presets
- Generated: No
- Committed: Yes
- Format: `.environment` files are JSON serialized `EnvironmentDefinition` structs

**`assets/shaders/`:**
- Purpose: GLSL source code
- Generated: No
- Committed: Yes
- Format: `.vert`, `.frag` files (GLSL 4.10)

**`editor_layouts/`:**
- Purpose: ImGui dock layout presets (ImGui format)
- Generated: Yes (saved from editor via "Save Layout")
- Committed: Yes (useful for collaboration)
- Auto-loaded on editor startup if matching layout name selected

**`tests/data/`:**
- Purpose: Test fixtures (models, scenes, materials)
- Generated: No (curated for test coverage)
- Committed: Yes
- Examples: `tests/data/models/`, `tests/data/scenes/`

**`.planning/phases/`:**
- Purpose: GSD workflow — phase planning documents
- Generated: Dynamically by `/gsd:plan-phase` command
- Committed: Yes
- Format: Markdown with implementation tasks and test plans

**`.planning/codebase/`:**
- Purpose: GSD reference docs for code execution
- Generated: Dynamically by `/gsd:map-codebase` command
- Committed: Yes
- Files: ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, STACK.md, INTEGRATIONS.md, CONCERNS.md

## Organizational Issues

**Layer Boundary Violation:**
- `src/engine/ui/ImGuiLayer.h` includes `game/rendering/RuntimeLightingOverride.h`
- Impact: Engine should not depend on game
- Fix: Extract `RuntimeLightingOverride` into a shared data struct in `engine/rendering/`, or pass as opaque parameter
- Severity: Medium — limits engine reusability but doesn't break functionality

**Tight Coupling in RenderSystem:**
- `src/game/systems/RenderSystem.h` imports `engine/ui/ImGuiLayer.h` directly
- Creates circular dependency potential if engine_ui ever needs game types
- Fix: Pass ImGuiLayer via injection rather than storing it
- Severity: Low — currently works but limits extensibility

**Missing Integration Tests:**
- `tests/` has minimal integration testing between systems
- Example: No test for "load level → spawn entities → render frame → verify output"
- Fix: Add `tests/integration/` with end-to-end scenarios
- Severity: Low — system design is sound, but coverage could be better

## Module Organization Strengths

1. **Clear layer separation**: Engine has zero game dependencies; Editor clearly depends on Game
2. **Component-based design**: POD components + ECS queries make systems decoupled
3. **Procedural content**: Levels, materials, environments are data-driven, not hard-coded
4. **Simulation containers**: `RuntimeGameSession` allows multiple independent play sessions
5. **Hot-reload support**: Material/environment definitions reload at runtime in editor
6. **Extensible systems**: New gameplay features easily added as new System subclasses

---

*Structure analysis: 2026-04-05*
