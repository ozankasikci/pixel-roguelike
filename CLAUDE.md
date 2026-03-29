<!-- GSD:project-start source:PROJECT.md -->
## Project

**3D Roguelike**

A first-person 3D roguelike with a distinctive 1-bit dithered black-and-white visual style, built on a custom C++ engine. The player explores gothic cathedral-like environments, fighting enemies with melee and ranged weapons, progressing through handcrafted levels with escalating difficulty and boss encounters.

**Core Value:** The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable. If the look doesn't land, nothing else matters.

### Constraints

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: OpenGL 4.1 Core Profile (macOS caps at 4.1; all shaders target GLSL 4.10)
- **Visual style**: Strictly 1-bit (black and white) — no grayscale, no color
- **Platform**: Desktop (Windows/macOS/Linux)
<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->
## Technology Stack

## Recommended Stack
### Core Technologies
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++20 | C++20 standard | Implementation language | `std::span`, concepts, designated initializers, and constexpr improvements pay off in engine code; EnTT 3.x requires C++20; broadly supported on all target compilers (MSVC 2019+, GCC 10+, Clang 12+) |
| CMake | 3.28+ | Build system | De-facto standard for cross-platform C++ projects; best ecosystem support for GLFW, Jolt, EnTT; FetchContent simplifies vendoring; VS Code and CLion integration is first-class |
| OpenGL 4.1 Core Profile | 4.1 | Graphics API | Practical ceiling set by macOS (Apple deprecated OpenGL at 4.1); custom 1-bit post-process shader is a simple fullscreen quad blit; Vulkan complexity would dominate the schedule without improving the dithered aesthetic |
| GLFW | 3.4 (released Feb 2024) | Window creation, OpenGL context, input | Focused and minimal — exactly what a custom engine needs; callback-based input maps cleanly to an event bus architecture; 12ms window creation vs SDL's 45ms; no audio/networking bloat; 3.4 adds runtime platform selection and Wayland support |
| GLAD 2 | v2.0.8 (released Sep 2025) | OpenGL function loader | Modern replacement for GLEW; generates only the extension set you request; supports OpenGL Core profile natively without the `glewExperimental` workaround GLEW needs; GLAD 2 adds Vulkan/EGL support if the API is ever extended |
| GLM | 1.0.3 (released Dec 2025) | Math: vectors, matrices, quaternions | Header-only; syntax mirrors GLSL exactly, which matters when writing shader-side math alongside CPU-side transforms; the only maintained C++ math lib with full GLSL spec coverage; 1.0.x branch supports C++17/20 |
| EnTT | v3.16.0 (released Nov 2025) | Entity-Component System (ECS) | Used in Minecraft; header-only; archetype-sparse-set hybrid delivers cache-friendly iteration; component queries are composable at compile time; forces clean separation between game state and rendering that makes the dithering post-pass trivial to layer on top |
### Post-Processing / Rendering Support
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| GLSL (via OpenGL) | 4.10 | Shader language | All shaders use `#version 410 core` (macOS ceiling); the 1-bit dithering shader is a fragment shader operating on a fullscreen texture; GLSL is the only choice when targeting OpenGL |
| stb_image | 2.30 (latest as of master, 2024-07) | PNG/JPG texture loading | Single-header, zero-dependency, public domain; the standard choice for loading textures in custom engines; stb_image.h + STB_IMAGE_IMPLEMENTATION is all that is needed |
| stb_truetype | latest master | Font rasterization for HUD/debug text | Same family as stb_image; renders TTF glyphs to a bitmap atlas at startup; sufficient for in-game UI text in a roguelike |
### Physics / Collision
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Jolt Physics | v5.4.0 (released Sep 2025) | Rigid body physics, collision detection, raycasting | Modern C++17/20 design; multi-threaded; used in Horizon Forbidden West and Death Stranding 2; significantly better API design and performance than Bullet3; excellent documentation; MIT license; character controller included — important for first-person movement |
### Audio
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| OpenAL Soft | v1.25.1 (released Jan 2025) | 3D positional audio | Cross-platform implementation of the OpenAL API; spatial audio out of the box (torches crackling as the player approaches is a natural fit for the gothic setting); LGPL license; the community standard for spatial audio in custom C++ game engines |
### Development / Debugging
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Dear ImGui | v1.92.6 (released Feb 2025) | In-engine debug UI | Industry standard debug overlay; integrates with any OpenGL+GLFW setup via provided backends; invaluable for tweaking dither threshold values, inspecting ECS component state, and visualizing AI patrol paths at runtime |
| spdlog | v1.x latest | Structured logging | Header-only fast logger; fmt-backed formatting; used everywhere in the C++ gamedev community; category sinks let you filter rendering vs AI vs audio logs separately |
## The 1-Bit Dithering Shader (Key Technical Decision)
## Installation
# System dependencies (macOS with Homebrew)
# System dependencies (Ubuntu/Debian)
# GLAD: generate via the web tool or Python
# https://glad.dav1d.de/ — select OpenGL 4.1, Core profile, generate C/C++
# Copy glad.h and glad.c into your project
# CMake FetchContent (CMakeLists.txt) — pulls at configure time:
# GLM       https://github.com/g-truc/glm           (tag: 1.0.3)
# EnTT      https://github.com/skypjack/entt         (tag: v3.16.0)
# JoltPhysics https://github.com/jrouwe/JoltPhysics  (tag: v5.4.0)
# Dear ImGui  https://github.com/ocornut/imgui        (tag: v1.92.6)
# spdlog      https://github.com/gabime/spdlog        (tag: v1.15.x)
# stb headers — copy directly into the repo:
# https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
# https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h
## Alternatives Considered
| Recommended | Alternative | When to Use Alternative Instead |
|-------------|-------------|----------------------------------|
| OpenGL 4.1 | Vulkan | If GPU-driven rendering for thousands of draw calls is needed — not applicable here |
| OpenGL 4.1 | WebGPU (via wgpu/Dawn) | If a web/browser deployment target is added later |
| GLFW | SDL3 | SDL3's new GPU API is compelling if you need its audio, networking, and 2D renderer too; for a custom engine where each subsystem is deliberately chosen, GLFW's focus is a better fit |
| Jolt Physics | Bullet3 | Bullet3 is fine if you already have code using it; Jolt has a cleaner API and better documentation for greenfield; no reason to choose Bullet3 for a new project |
| OpenAL Soft | miniaudio | miniaudio is a simpler single-header solution; choose it if you need audio at all but want zero dependencies; OpenAL Soft wins when 3D positional audio matters, which it does for dungeon atmosphere |
| EnTT | Flecs | Flecs has a richer query language and built-in pipeline scheduling; switch to it if the ECS becomes the architectural bottleneck; EnTT is simpler to start with |
| CMake | xmake | xmake is cleaner syntax and includes package management; switch if CMake's verbosity becomes intolerable; xmake has a smaller ecosystem and some libraries don't yet provide native xmake support |
| Dear ImGui (debug only) | No in-game UI | For the final shipped game, ImGui is stripped out; the 1-bit aesthetic means any HUD is custom-rendered using stb_truetype glyphs — keep it minimal |
## What NOT to Use
| Avoid | Why | Use Instead |
|-------|-----|-------------|
| GLEW | Stagnant since 2.2.0 (2017); requires `glewExperimental = GL_TRUE` for Core profile contexts; no GLAD 2-era features | GLAD v2 |
| SDL2 audio subsystem | Mixed audio quality; audio is not SDL2's strength | OpenAL Soft |
| Assimp (for new code) | Already used (v6.0.4) for FBX import and multi-format support, but prefer tinygltf for new glTF-only loading paths — Assimp is heavy (1M+ lines) and fragile to build | tinygltf for new glTF paths; keep Assimp for FBX/legacy |
| Unity / Unreal / Godot | Explicitly out of scope per project requirements; the dithering post-pass needs direct FBO control that is difficult or impossible to achieve in managed pipelines | Custom C++ + OpenGL (as specified) |
| Bullet3 | Last meaningful update was 2022; development is effectively frozen; Jolt Physics is its modern successor | Jolt Physics v5 |
| OpenGL compatibility profile | Allows deprecated immediate-mode calls to leak in; makes porting to Vulkan harder; produces driver-specific behavior differences | OpenGL 4.1 Core Profile only |
| Boost | Enormous dependency for utility functions that C++20 STL now provides; adds significant compile time | C++20 STL (`std::span`, `std::ranges`, `std::format`) |
## Stack Patterns by Variant
- Replace OpenGL 4.1 with Metal (via the `metal-cpp` header-only wrapper from Apple)
- Or use MoltenVK to run Vulkan on Metal
- OpenGL on macOS is deprecated at 4.1 — Apple has not updated it since 2018
- Replace OpenGL 4.1 + GLFW with WebGPU (Emscripten's built-in WebGPU support)
- Replace OpenAL Soft with the Web Audio API via Emscripten
- EnTT, GLM, and Jolt all have WASM-compatible builds
- Pass the view-projection matrix to the dither shader
- Reconstruct world-space position from depth buffer in the post-pass fragment shader
- Use world-space XY (or XZ) coordinates modulo the Bayer matrix size as the threshold index
## Version Compatibility
| Package | Requires | Notes |
|---------|----------|-------|
| EnTT v3.16.0 | C++20 | The `entt::handle` API requires C++20 concepts |
| Jolt Physics v5.4.0 | C++17 minimum, C++20 recommended | Uses `std::span`, `std::bit_cast`; enable `JPH_USE_STD_VECTOR` for STL integration |
| GLM 1.0.3 | C++17 minimum | Branch 1.1 will require C++17; stick with 1.0.x if your CI enforces C++14 |
| Dear ImGui v1.92.6 | C++11 | No C++20 requirement; works with any GLFW+OpenGL backend |
| GLFW 3.4 | CMake 3.16+ | On macOS, requires `-framework Cocoa -framework OpenGL -framework IOKit` link flags |
| OpenAL Soft v1.25.1 | None (dynamic link) | Link as `openal` on Unix; `OpenAL32.lib` on Windows |
## Sources
- GLM GitHub releases — https://github.com/g-truc/glm/releases — version 1.0.3 confirmed (Dec 2025) — HIGH confidence
- GLFW release notes — https://www.glfw.org/docs/3.4/news.html — 3.4 features confirmed — HIGH confidence
- GLAD GitHub releases — https://github.com/Dav1dde/glad/releases — v2.0.8 confirmed (Sep 2025) — HIGH confidence
- Jolt Physics GitHub releases — https://github.com/jrouwe/JoltPhysics/releases — v5.4.0 confirmed (Sep 2025) — HIGH confidence
- Dear ImGui GitHub releases — https://github.com/ocornut/imgui/releases — v1.92.6 confirmed (Feb 2025) — HIGH confidence
- OpenAL Soft GitHub releases — https://github.com/kcat/openal-soft/releases — v1.25.1 confirmed (Jan 2025) — HIGH confidence
- EnTT GitHub releases — https://github.com/skypjack/entt/releases — v3.16.0 confirmed (Nov 2025) — HIGH confidence
- Obra Dinn dithering devlog — https://dukope.com/devlogs/obra-dinn/tig-32/ — post-process technique confirmed — HIGH confidence
- Daniel Ilett dither tutorial — https://danielilett.com/2020-02-26-tut3-9-obra-dithering/ — Bayer matrix GLSL approach confirmed — MEDIUM confidence
- OpenGL vs Vulkan comparison — https://thatonegamedev.com/cpp/opengl-vs-vulkan/ — recommendation reasoning — MEDIUM confidence
- GLAD vs GLEW — https://community.khronos.org/t/glad-vs-glew-for-extension-loading/111911 — GLAD recommendation confirmed — MEDIUM confidence
- Jolt vs Bullet comparison — https://forum.revolutionarygamesstudio.com/t/picking-a-physics-engine-for-thrive/1031 — MEDIUM confidence
- CMake/vcpkg/Conan comparison — https://matgomes.com/vcpkg-vs-conan-for-cpp/ — MEDIUM confidence
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

### Naming
- **Classes/Structs**: `PascalCase` — `TransformComponent`, `PhysicsSystem`, `RuntimeGameSession`
- **Functions/Methods**: `camelCase` — `isKeyPressed()`, `setCharacterVelocity()`, `loadFromFile()`
- **Variables**: `snake_case` generally; private members use trailing underscore — `window_`, `registry_`
- **Constants**: `k` prefix + PascalCase — `kMaxRenderLights`, `kMaterialKindCount`
- **Enums**: `PascalCase` enum class with `PascalCase` values — `enum class MaterialKind { Stone, Wood, Metal }`
- **Files**: `PascalCase.h` / `PascalCase.cpp` matching the primary class name

### Headers & Includes
- `#pragma once` (no include guards)
- Include order: standard library → third-party (GLAD, GLM, EnTT, spdlog) → project headers
- Project headers use paths relative to `src/` root

### Class Patterns
- Non-copyable by default (explicitly `= delete` copy ctor/assignment)
- Move semantics where ownership transfer is needed
- Pimpl for complex implementations hiding third-party types (`PhysicsSystem` hides Jolt internals)
- RAII for OpenGL resources (destructors clean up VAO/VBO/EBO/FBO)
- Virtual destructors on all base classes (`virtual ~Class() = default;`)

### ECS
- Components are plain-old-data structs (no methods, no inheritance)
- Marker/tag components for entity identification: `PlayerTag`, `ControllableTag`, `PrimaryCameraTag`
- Systems inherit from `System` base class with `init()`, `update()`, `shutdown()`
- Systems are registered with an `UpdatePhase` for execution ordering

### Code Style
- `.clang-format` based on LLVM: 4-space indent, 100-char column limit, attached braces
- Pointer alignment: left (`int* ptr`)
- No short functions on single line

### Build
- CMake with `FetchContent` for most deps; GLFW, GLM, spdlog via Homebrew
- Custom `pixel_roguelike_add_test()` function for test registration
- Tests are standalone executables (no external test framework) — exit code = pass/fail
- Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`

### Shaders
- GLSL version `#version 410 core` (macOS maximum)
- Engine shaders in `assets/shaders/engine/`, game shaders in `assets/shaders/game/`
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

### Layered Design
Three layers with strict dependency direction: **Engine → Game → Editor**

### CMake Target Graph
```
pixel-roguelike ──→ gameplay ──→ game_rendering ──→ game_content ──→ engine_rendering ──→ engine_core
level-editor ──→ editor ──→ gameplay                                   engine_ui
procedural-model-viewer ──→ game_rendering                             engine_scene
                                                                       engine_input
                                                                       engine_physics
```

### Engine Layer (`src/engine/`)
- **core/**: `Application` (main loop, system/service registration), `Window` (GLFW), `EventBus` (type-safe pub/sub), `Time`
- **ecs/**: `Registry.h` — thin EnTT wrapper
- **rendering/core/**: `Shader`, `Framebuffer`
- **rendering/geometry/**: `Mesh` (RAII VAO/VBO), `MeshLibrary` (named registry), `Renderer`
- **rendering/assets/**: `AssimpLoader`, `GltfLoader`, `Texture2D`, `TextureCube`
- **rendering/lighting/**: `RenderLight`, `ShadowMap`
- **rendering/post/**: `CompositePass`, `StylizePass`, `SkySettings`
- **input/**: `InputSystem` — GLFW callbacks, cursor lock, accumulated `RuntimeInputState`
- **physics/**: `PhysicsSystem` — Jolt character controller behind pimpl
- **scene/**: `SceneManager`
- **ui/**: `ImGuiLayer`, `Screenshot`

### Game Layer (`src/game/`)
- **components/**: POD structs — `TransformComponent`, `MeshComponent`, `LightComponent`, `CameraComponent`, `CharacterControllerComponent`, `StaticColliderComponent`, `InteractableComponent`, `DoorComponent`, `CheckpointComponent`, `PlayerMovementComponent`, `ViewmodelComponent`, tags
- **content/**: `ContentRegistry` — loads and resolves `MaterialDefinition`, `EnvironmentDefinition`, `WeaponDefinition`, `EnemyDefinition`, `GameplayArchetypeDefinition`
- **level/**: `LevelDef` (data), `LevelBuilder` (spawns entities from def), `LevelLoader` (file I/O)
- **rendering/**: `RuntimeSceneRenderer`, `MaterialLibrary`, `EnvironmentSync`, `MaterialKind` enum (Stone/Wood/Metal/Wax/Moss/Viewmodel/Floor/Brick)
- **systems/**: `PlayerMovementSystem`, `CameraSystem`, `RenderSystem`, `DoorSystem`, `CheckpointSystem`, `InteractionSystem`, `InventorySystem`
- **runtime/**: `RuntimeGameSession` (play-session orchestrator), `RuntimeGameplay`, `RuntimeInputState`
- **session/**: `RunSession` (persistent state), `EquipmentState`
- **scenes/**: `CathedralScene`, `SilosCloisterScene`
- **prefabs/**: `GameplayPrefabs` — spawns composite entities (doors, checkpoints)

### Editor Layer (`src/editor/`)
- **scene/**: `EditorSceneDocument` — scene graph with parent/child hierarchy, world transforms, serialization
- **core/**: `EditorCommand` (undo/redo), `LevelEditorCore` (dock layout), layout presets, preview sessions
- **viewport/**: `EditorViewportController` — camera orbit, ImGuizmo gizmos
- **ui/**: Inspector, outliner, environment panel, asset browser
- **render/**: Scene and asset preview renderers

### Execution Model
Systems execute in phase order per frame:
`Input → Interaction → Physics → Gameplay → Camera → Render`

Services are registered via type-safe service locator on `Application` (`emplaceService<T>()` / `getService<T>()`).

### Key Data Flows
- **Level loading**: `.scene` file → `LevelDef` struct → `LevelBuilder` spawns ECS entities → systems operate on components
- **Rendering**: `RuntimeSceneRenderer` queries ECS for mesh/light/camera components → scene pass to FBO → stylize post-pass → composite to screen
- **Materials**: `ContentRegistry` resolves `MaterialDefinition` (with parent inheritance) → shader uniforms set per `MaterialKind`
- **Editor round-trip**: `EditorSceneDocument` ↔ `LevelDef` ↔ `.scene` file; preview spawns temporary `RuntimeGameSession`

### Assets Layout
```
assets/
├── meshes/       .glb files (arch, pillar, door, hand, dagger)
├── scenes/       .scene files (cathedral, silos_cloister)
├── prefabs/      .prefab files (checkpoint, double_door)
├── shaders/
│   ├── engine/   composite, shadow_depth, stylize (post-process)
│   └── game/     scene.vert/frag (PBR-like, 32 lights, shadow maps, procedural textures)
├── skies/        Cubemap PNGs, horizon TGAs, cloud overlays
└── fonts/        JetBrainsMono for editor
```
<!-- GSD:architecture-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd:quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd:debug` for investigation and bug fixing
- `/gsd:execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd:profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
