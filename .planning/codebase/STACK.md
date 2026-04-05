# Technology Stack

**Analysis Date:** 2026-04-05

## Languages

**Primary:**
- C++20 - Core engine and game implementation language. Required by EnTT 3.16.0; provides `std::span`, concepts, designated initializers, and constexpr improvements critical for engine code.

**Secondary:**
- C - Vorbis audio codec via `stb_vorbis.c` (single-file implementation)
- GLSL 4.10 - All shaders target OpenGL 4.1 Core Profile (`#version 410 core`), macOS ceiling

## Runtime

**Environment:**
- macOS (primary development target) with Cocoa framework
- Windows/Linux support via OpenGL 4.1 Core (cross-platform)
- GLFW 3.4 for window management and context initialization
- Desktop applications only (no mobile/web)

**Compiler Support:**
- MSVC 2019+, GCC 10+, Clang 12+
- CMake 3.28+ required (for FetchContent, GLAD 2, modern target properties)

## Frameworks

**Core Engine:**
- EnTT v3.16.0 (header-only) - Entity-Component System (ECS). Archetype-sparse-set hybrid provides cache-friendly iteration. No runtime registration needed. See `src/engine/ecs/Registry.h` thin wrapper.
- Jolt Physics v5.4.0 - Rigid body dynamics, collision detection, character controller. Wrapped in pimpl pattern at `src/engine/physics/PhysicsSystem.h/cpp`.
- OpenGL 4.1 Core Profile via GLAD 2 v2.0.8 - Graphics rendering
- GLFW 3.4 - Window creation, OpenGL context, input event callbacks

**UI/Debug:**
- Dear ImGui v1.92.6-docking - Debug overlay and editor UI. Compiled as static library with GLFW+OpenGL3 backends at `src/engine/ui/ImGuiLayer.h`.
- ImGuizmo - 3D transform gizmos for editor. Vendored at `/external/ImGuizmo/`.

**Math & Graphics:**
- GLM 1.0.3 (header-only) - Vectors, matrices, quaternions. Syntax mirrors GLSL exactly.
- tinygltf v2.9.3 (header-only) - glTF 2.0 format loading/writing. Used by `src/engine/rendering/assets/GltfLoader.h`.

**Asset Loading:**
- Assimp v6.0.4 - Multi-format model importing (FBX primary use case). Static library, wrapped at `src/engine/rendering/assets/AssimpLoader.h`. Superceded by tinygltf for new glTF-only paths.
- stb_image (via tinygltf) - PNG/JPG texture loading, single-header library used in `src/engine/rendering/assets/Texture2D.cpp` and `TextureCube.cpp`.
- stb_image_write - PNG screenshot export, single-header at `external/stb_image_write.h`, used by `src/engine/ui/Screenshot.cpp`.
- stb_vorbis - OGG Vorbis audio decoding, single C file at `external/stb_vorbis.c`, compiled as part of `engine_audio` library.

**Logging:**
- spdlog v1.x (header-only) - Fast structured logging. Category sinks allow filtering by subsystem (rendering, AI, audio). Used throughout engine.

**Data Serialization:**
- nlohmann/json v3.11.3 (header-only) - JSON parsing/generation. Used by editor debug harness at `src/editor/debug/` for Unix socket protocol (line-delimited JSON commands).

**Audio:**
- OpenAL Soft v1.25.1 - 3D positional audio. Homebrew keg-only version preferred on macOS (CMake detects at `/opt/homebrew/opt/openal-soft`). System OpenGL/Cocoa frameworks required on macOS.

## Configuration

**Build System:**
- CMake 3.28+ with FetchContent for dependency management
- Dependencies fetched at configure time: GLAD, EnTT, Jolt Physics, Dear ImGui, tinygltf, Assimp, nlohmann_json
- System packages (GLFW, GLM, spdlog, OpenAL Soft) resolved via Homebrew + `find_package()`
- Python 3 + jinja2 required for GLAD code generation (isolated in `.venv`)

**Compiler Settings:**
- `.clang-format` (LLVM style) enforced: 4-space indent, 100-char column limit, left pointer alignment, attached braces
- `#pragma once` (no include guards) for all headers
- Include order: STL → third-party (GLAD, GLM, EnTT, spdlog) → project headers
- `SortIncludes: Never` preserves manual ordering

**Environment:**
- Project config file: `project.cfg` (key=value format, currently stores `last_scene`)
- Read/write via `ProjectConfig.h` in `src/engine/core/`
- No `.env` files detected in source tree (environment setup via CMake/Homebrew)

**Platform-Specific:**
- macOS: `-framework Cocoa -framework OpenGL -framework IOKit` linker flags via `cmake/DesktopApp.cmake`
- OpenAL Soft path detection for Homebrew keg-only installation
- zlib detection for Assimp on macOS (Homebrew path)

## Build Artifacts

**Executables (3 targets):**
- `pixel-roguelike` - Game runtime (`apps/runtime/main.cpp`). Links: `engine_scene`, `gameplay`.
- `level-editor` - Scene editor (`apps/level_editor/main.cpp`). Links: `editor`.
- `procedural-model-viewer` - Asset preview tool (`apps/model_viewer/main.cpp`). Links: `game_rendering`.

**Static Libraries (layered dependency graph):**
```
engine_core
  ├─ GLAD, GLFW, spdlog, EnTT
  └─ Provides: Window, Application, EventBus, Time, core services

engine_rendering
  ├─ engine_core, GLM, tinygltf (PUBLIC), Assimp (PRIVATE)
  └─ Provides: Shader, Framebuffer, Mesh, MeshLibrary, Renderer, texture/light systems, asset loaders

engine_ui
  ├─ engine_rendering, ImGui (+ GLFW, OpenGL3 backends)
  └─ Provides: ImGuiLayer, Screenshot

engine_scene
  ├─ engine_core
  └─ Provides: SceneManager

engine_input
  ├─ engine_core, ImGui, GLM
  └─ Provides: InputSystem, ActionMap, GLFW callbacks

engine_physics
  ├─ engine_core, GLM
  └─ Links: Jolt (PRIVATE, hidden by pimpl)
  └─ Provides: PhysicsSystem with character controller API

engine_audio
  ├─ engine_core, GLM, OpenAL Soft (PRIVATE), stb_vorbis.c (PRIVATE)
  └─ Provides: AudioSystem (3D positional SFX, music/ambient streaming)

game_content
  ├─ engine_rendering
  └─ Provides: ContentRegistry, MaterialDefinition, EnvironmentDefinition

game_rendering
  ├─ game_content, engine_ui, tinygltf (PRIVATE)
  └─ Provides: RuntimeSceneRenderer, MaterialLibrary, MaterialKind enum

gameplay
  ├─ game_content, game_rendering, engine_audio, engine_input, engine_physics, EnTT
  └─ Provides: Systems (movement, camera, render, doors, checkpoints, etc.), RuntimeGameSession, LevelBuilder

editor
  ├─ gameplay, nlohmann_json, ImGuizmo
  └─ Provides: Scene graph, gizmos, inspector, outliner, debug harness
```

## Version Pinning & Dependency Health

**Pinned Versions (FetchContent):**
- GLAD: v2.0.8 (stable, generated locally)
- EnTT: v3.16.0 (latest stable, requires C++20)
- Jolt Physics: v5.4.0 (latest, Sep 2025)
- Dear ImGui: v1.92.6-docking (docking branch, not main; updated Feb 2025)
- tinygltf: v2.9.3 (latest stable, 2024)
- Assimp: v6.0.4 (pinned; last meaningful update 2022, effectively frozen)
- nlohmann_json: v3.11.3 (latest stable, 2024)

**System Packages (Homebrew):**
- GLFW 3.4 (minimum version constraint: 3.4 REQUIRED)
- GLM (any version, no constraint in CMakeLists.txt)
- spdlog (any version)
- OpenAL Soft v1.25.1 (detected keg-only path on macOS)

**Build Health:**
- FetchContent reproducibility enabled (GIT_PROGRESS, GIT_SHALLOW for Assimp)
- Transitive dependencies handled: Assimp pulls zlib (Homebrew override provided), Jolt is self-contained
- BUILD_SHARED_LIBS temporarily forced OFF for Assimp, saved/restored to avoid side effects

## Platform Requirements

**Development:**
- macOS 12+ (primary) or Ubuntu/Debian with OpenGL 4.1 support
- Python 3 + venv for GLAD code generation
- CMake 3.28+
- C++20 compiler

**Runtime:**
- macOS, Windows (MSVC 2019+), Linux (GCC 10+)
- OpenGL 4.1 Core compatible GPU
- No runtime dependencies beyond system libraries (all libs vendored or built static)

---

*Stack analysis: 2026-04-05*
