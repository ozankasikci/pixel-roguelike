# Technology Stack

**Analysis Date:** 2026-04-01

## Languages

**Primary:**
- C++20 — All engine, game, and editor source code. C++20 features in active use: `std::span`, `std::any`, designated initializers, `std::type_index`, `std::filesystem`. Required by EnTT v3.16.0.
- GLSL 4.10 — All shaders. Version pinned at `#version 410 core` throughout because macOS OpenGL caps at 4.1.

**Secondary:**
- C — `external/stb_vorbis.c` compiled as C (not C++) via CMake source property override; `external/stb_image_write.h` (header-only, vendored).
- Python 3.13 — Tooling only. Used by GLAD 2's CMake generation step (requires `jinja2`). Also one mesh generation script (`tools/generate_hand_with_old_dagger.py`). Managed via `.venv/` (Python 3.13.7 from Homebrew).

## Runtime

**Environment:**
- Desktop native — Windows, macOS, Linux
- macOS is the primary development platform (Apple Silicon, Homebrew at `/opt/homebrew/`)

**Package Manager:**
- Homebrew (macOS) — for system-level packages: GLFW 3.4, GLM, spdlog, OpenAL Soft, zlib
- CMake FetchContent — for source-built deps: GLAD 2, EnTT, Jolt, Dear ImGui, tinygltf, Assimp
- No lockfile for FetchContent deps; all are pinned by exact `GIT_TAG` in `CMakeLists.txt`

## Frameworks

**Core:**
- CMake 3.28+ — Build system. `CMakeLists.txt` at root; sub-targets in `src/engine/`, `src/game/`, `src/editor/`, `apps/`, `tests/`, `tools/`
- Custom C++ engine — No Unity/Unreal/Godot; all subsystems hand-written

**Testing:**
- No external test framework — Tests are standalone executables; pass/fail via exit code. CTest runner via `cd build && ctest --output-on-failure`. Custom CMake function `pixel_roguelike_add_test()` defined in `tests/cmake/TestSupport.cmake`. Also `pixel_roguelike_add_profile()` for profiling executables.

**Build/Dev:**
- Makefile (thin wrapper) — `make configure`, `make editor`, `make game`, `make viewer`, `make test`, `make clean`
- clang-format (LLVM style, 4-space indent, 100-char limit) — config at `.clang-format`
- Python venv — `.venv/` for GLAD CMake dependency (`jinja2`)

## Key Dependencies

**Via CMake FetchContent (source-built, pinned by GIT_TAG):**

| Library | Tag | Purpose |
|---------|-----|---------|
| GLAD 2 | `v2.0.8` | OpenGL 4.1 Core Profile function loader. Generated as `glad_gl` static lib via `glad_add_library(glad_gl STATIC REPRODUCIBLE LOADER API gl:core=4.1)`. Requires Python + jinja2. |
| EnTT | `v3.16.0` | Entity Component System. Header-only; linked as `EnTT::EnTT`. Direct dependency of `engine_core` and `gameplay`. |
| Jolt Physics | `v5.4.0` | Rigid body physics + character controller. Builds with all sample/test targets disabled. Linked privately into `engine_physics` (pimpl boundary). |
| Dear ImGui | `v1.92.6-docking` | Debug overlay + editor UI. Uses docking branch. Manually compiled as a static `imgui` target from fetched sources including `imgui_impl_glfw.cpp` and `imgui_impl_opengl3.cpp`. |
| tinygltf | `v2.9.3` | Header-only glTF 2.0 loader. Used by `engine_rendering` and `game_rendering`. stb_image integration disabled (`TINYGLTF_NO_STB_IMAGE`). |
| Assimp | `v6.0.4` | Multi-format model importer for FBX and legacy formats. Built as static lib; tools/docs/tests/zlib all disabled. `BUILD_SHARED_LIBS` forcibly set to OFF and restored after. macOS: uses Homebrew zlib at `/opt/homebrew/opt/zlib`. |

**Via Homebrew (system-installed, found by CMake `find_package`):**

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.4 | Window creation, OpenGL context, input callbacks |
| GLM | (current Homebrew) | Header-only math: vectors, matrices, quaternions — GLSL-mirroring API |
| spdlog | (current Homebrew) | Fast structured logger; `spdlog::spdlog` target used throughout |
| OpenAL Soft | 1.25.1 | 3D positional audio. On macOS, explicitly pointed to Homebrew keg-only path `/opt/homebrew/opt/openal-soft/` to avoid the macOS system framework. |

**Vendored directly in `external/`:**

| File | Purpose |
|------|---------|
| `external/stb_vorbis.c` | OGG Vorbis streaming decoder. Compiled as C source into `engine_audio`. Used for music and ambient streaming. |
| `external/stb_image_write.h` | PNG/BMP screenshot writing. Used by `engine/ui/Screenshot.cpp`. |
| `external/ImGuizmo/ImGuizmo.cpp` + `ImGuizmo.h` | 3D transform gizmos in editor viewport. Compiled directly into the `editor` static library. |

**Pulled in via tinygltf (stb_image):**
- `stb_image.h` — Texture loading (PNG/JPG). Used by `engine/rendering/assets/Texture2D.cpp`, `TextureCube.cpp`, and `game/rendering/MaterialTextureLibrary.cpp`. The implementation define `STB_IMAGE_IMPLEMENTATION` is provided by tinygltf, not manually.

**Font assets (TTF, not a code dependency):**
- `assets/fonts/editor/JetBrainsMono-Variable.ttf`, `Inter-Variable.ttf`, `Roboto-Variable.ttf` — loaded at runtime by `ImGuiLayer` for editor font presets.

## CMake Target Graph

```
pixel-roguelike (apps/runtime)
  └── engine_scene, gameplay

level-editor (apps/level_editor)
  └── editor

procedural-model-viewer (apps/model_viewer)
  └── game_content, engine_ui

editor
  └── gameplay, ImGuizmo (vendored)

gameplay
  └── game_content, game_rendering, engine_audio, engine_input, engine_physics

game_rendering
  └── game_content, engine_ui, tinygltf

game_content
  └── engine_rendering

engine_rendering
  └── engine_core, glm, tinygltf (private), assimp (private)

engine_ui
  └── engine_rendering, imgui

engine_physics
  └── engine_core, glm, Jolt (private)

engine_audio
  └── engine_core, glm, stb_vorbis.c (private), OpenAL (private)

engine_core
  └── glad_gl, glfw, spdlog, EnTT

mesh_generator (tools)
  └── glm, tinygltf
```

## Configuration

**Build:**
- Source: `CMakeLists.txt` (root), sub-targets in `src/engine/`, `src/game/`, `src/editor/`, `apps/*/`, `tests/`, `tools/`
- Shared include root: `src/` and `external/` injected by `configure_desktop_app()` in `cmake/DesktopApp.cmake`
- macOS-specific link flags added by `configure_desktop_app()`: `-framework Cocoa`, `-framework OpenGL`, `-framework IOKit`, plus `GL_SILENCE_DEPRECATION` define
- Build directory: `build/` (per Makefile convention); also `build-test/` tracked in gitignore

**Runtime project config:**
- `assets/project.cfg` — key=value file; currently stores only `last_scene` (the scene to open on launch). Read/written by `ProjectConfig.cpp`.

**Asset cache:**
- `AssetCache` writes processed mesh and texture data to a disk cache directory (FNV-1a 64-bit hash keyed). Cache root resolved by `AssetCache::cacheRoot()`.

**No environment variables or secrets** — All configuration is file-based or compiled-in. No `.env` files present.

## Platform Requirements

**Development:**
- macOS (Apple Silicon): Homebrew at `/opt/homebrew/`; install `glfw`, `glm`, `spdlog`, `openal-soft`, `zlib`
- Python 3.13 + venv with `jinja2` (for GLAD CMake generation): `python3 -m venv .venv && .venv/bin/pip install jinja2`
- CMake 3.28+, a C++20 compiler (Clang 12+, GCC 10+, MSVC 2019+)

**Production:**
- Desktop binary: macOS, Windows, Linux
- Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`
- Assets directory must be present relative to the executable at runtime (`assets/`)

---

*Stack analysis: 2026-04-01*
