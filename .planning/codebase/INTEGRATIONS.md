# External Integrations

**Analysis Date:** 2026-04-05

## Graphics & Rendering Integration

**OpenGL 4.1 Core Profile:**
- Entry point: `src/engine/rendering/core/Shader.h`, `src/engine/rendering/core/Framebuffer.h`
- GLAD 2 v2.0.8 generates function pointers
- All shaders use `#version 410 core` (macOS ceiling, set in CMakeLists.txt line 20)
- GLSL math types use GLM header-only library (syntax mirrors GLSL)
- Abstraction: Non-copyable RAII classes manage GPU resources (VAO, VBO, FBO, program IDs)
- Wrapping pattern: Direct `#include <glad/gl.h>` used in Shader/Framebuffer, no intermediate wrapper layer
- Renderer at `src/engine/rendering/geometry/Renderer.h/cpp` queries scene graph and batches draw calls

**Post-Processing Pipeline:**
- Fullscreen fragment shader passes for stylization and composition
- `src/engine/rendering/post/` contains StylizePass, CompositePass, BloomPass, SsaoPass
- Custom lighting via cascaded shadow maps and LTC area lights (see `src/engine/rendering/lighting/`)

## Asset Loading & Format Support

**3D Model Formats:**

**glTF 2.0 (Primary):**
- Library: tinygltf v2.9.3 (header-only)
- Wrapper: `src/engine/rendering/assets/GltfLoader.h/cpp`
- API: `GltfLoader::load(filepath)` returns `std::unique_ptr<Mesh>`, or `loadRaw()` for CPU-side MeshGeometry
- Auto-generates missing normals
- Used by game asset pipeline for `.glb` files
- tinygltf also provides `stb_image.h` for embedded texture decoding

**FBX & Multi-Format (Legacy):**
- Library: Assimp v6.0.4 (static library)
- Wrapper: `src/engine/rendering/assets/AssimpLoader.h/cpp`
- API: `AssimpLoader::load(filepath)` returns `std::unique_ptr<Mesh>`, or `loadRaw()` / `loadRawMulti()`
- Handles FBX import with material name grouping
- Superceded by tinygltf for new glTF-only paths (lighter dependency)
- CMakeLists.txt (lines 89-121): Assimp built static, demo/samples/tests disabled

**Texture Loading:**
- Library: stb_image (bundled with tinygltf or imported separately)
- Used by: `src/engine/rendering/assets/Texture2D.cpp`, `TextureCube.cpp`
- API: Direct `stb_image.h` inclusion, `stbi_load()` for PNG/JPG → raw RGBA pixels
- Screenshot export: `stb_image_write.h` (single-header, at `external/stb_image_write.h`)

**Mesh Management:**
- `MeshLibrary` at `src/engine/rendering/geometry/MeshLibrary.h` — named registry of loaded meshes
- `AssetCache` at `src/engine/rendering/assets/AssetCache.h` — LRU cache with eviction policy

## Audio System Integration

**OpenAL Soft v1.25.1:**
- API: Native OpenAL C headers
- Wrapper: `src/engine/audio/AudioSystem.h/cpp`
- Pimpl pattern hides OpenAL internals (non-copyable, unique_ptr impl_)
- CMakeLists.txt (lines 71-77): Custom Homebrew path detection for macOS keg-only installation
- Private link target: OpenAL library resolved at configure time

**Audio Format Support:**

**WAV (Preloaded SFX):**
- Handled by OpenAL Soft directly (native format)
- API: `AudioSystem::loadSound(path)` → uint32_t handle, `playSound(handle, position, volume, pitch)`
- Positional audio: 3D source positions, listener transforms updated per frame
- Refundance & max distance parameters for attenuation falloff

**OGG Vorbis Streaming:**
- Library: stb_vorbis (single C file at `external/stb_vorbis.c`)
- Compiled as part of `engine_audio` target (CMakeLists.txt line 71)
- Set as C language to avoid C++ name mangling (line 78)
- API: `AudioSystem::playMusic(path, loop)`, `playAmbient(path, loop)`
- Streamed in 64KB chunks to avoid memory bloat
- Music & ambient have separate volume controls

**Volume Management:**
- Four independent categories: master, sfx, music, ambient
- Float range 0.0-1.0, real-time mixing via OpenAL gain property

**Listener Transform:**
- Called each frame by `AudioListenerSystem` (see `src/game/systems/AudioListenerSystem.cpp`)
- Sets OpenAL listener position, forward, up vectors for spatial audio

## Physics Integration

**Jolt Physics v5.4.0:**
- Library: Downloaded at CMakeLists.txt line 36-42, built as static library (all examples/tests disabled)
- Wrapper: `src/engine/physics/PhysicsSystem.h/cpp`
- Pimpl pattern completely hides Jolt API (no Jolt includes in public headers)
- Implementation detail: `PhysicsSystem::Impl` struct holds `JPH::PhysicsSystem`, character controller state

**Character Controller API (Hidden):**
- `setCharacterVelocity(entity, velocity)`
- `getCharacterPosition(entity)`, `setCharacterPosition(entity, position)`
- `getCharacterGroundState(entity)` → GroundState enum (OnGround, OnSteepGround, InAir)
- `updateCharacter(entity, deltaTime, gravity)` — physics tick called during Physics phase

**Collision Shapes:**
- Box & Cylinder colliders defined in level data
- `src/game/components/ColliderComponent.h` — ColliderShape enum (Box, Cylinder)
- Physics constraints built from LevelDef collider placements during LevelBuilder spawn

**Integration Pattern:**
- Game layer never directly touches Jolt types
- All physics queries go through PhysicsSystem public API
- ECS components store entity handles; PhysicsSystem maintains internal Jolt body map

## Input Integration

**GLFW 3.4:**
- Window creation & OpenGL context: `src/engine/core/Window.h/cpp`
- Event callbacks: `src/engine/input/InputSystem.h/cpp`
- Static callback approach: InputSystem stores `instance_` pointer, forwards GLFW events to instance methods
- Callbacks implemented: `cursorPosCallback`, `scrollCallback`, `keyCallback`, `charCallback`, `dropCallback` (for scene file drag-drop)

**Input Abstraction:**
- Accumulated state: `currentKeys_`, `previousKeys_` arrays (kMaxKeys = 512)
- Per-frame poll: `isKeyPressed()`, `isKeyJustPressed()`, `isKeyJustReleased()`
- Mouse movement: `mouseDelta()`, `mousePosition()`, `scrollDelta()`
- Cursor locking via GLFW (for FPS camera movement)

**ImGui Integration:**
- ImGui v1.92.6-docking backend for GLFW + OpenGL3
- InputSystem exposes `wantsCaptureMouse()` to check if ImGui consumed input

**Action Mapping:**
- `ActionMap` class at `src/engine/input/ActionMap.h` for rebindable input actions (D-03 requirement)

## Windowing & Desktop Environment

**GLFW 3.4:**
- Window title, width, height configured at Application construction
- Event polling via `GLFWwindow* handle()` and `swapBuffers()` / `pollEvents()`
- Platform-specific: `-framework Cocoa`, `-framework IOKit` linked on macOS (lines 8-12 in DesktopApp.cmake)
- File drag-drop support: `takeDroppedPaths()` returns filesystem paths dropped onto window

## UI & Editor Integration

**Dear ImGui v1.92.6-docking:**
- Compiled from source in CMakeLists.txt (lines 44-64) as static library
- Backends: `imgui_impl_glfw.cpp` + `imgui_impl_opengl3.cpp`
- Wrapper: `src/engine/ui/ImGuiLayer.h/cpp` manages initialization, frame begin/end, theme/font presets
- Theme presets: WarmStudioDark, SpectrumInspiredDark, GraphiteDense, etc.
- Font presets: SystemSans, JetBrainsMono (for editor debug text)

**ImGuizmo Integration:**
- Vendored at `external/ImGuizmo/` (117KB C++ implementation)
- Compiled as part of `editor` target (CMakeLists.txt line 30)
- Provides 3D transform gizmos (translate, rotate, scale) for editor viewport
- Matrices passed in/out; no persistence layer (editor handles state)

**Debug Harness (JSON IPC):**
- Unix socket server at `src/editor/debug/DebugServer.h/cpp`
- Wire protocol: Line-delimited JSON commands (33 commands total)
- Libraries: nlohmann_json v3.11.3 for parsing/generation
- Socket path: `/tmp/pixel-roguelike-editor-{pid}.sock`
- Enables programmatic editor control, inspection, undo/redo replay without manual reproduction

## Entity-Component System Integration

**EnTT v3.16.0:**
- Header-only library
- Wrapper: Thin `src/engine/ecs/Registry.h` around `entt::registry`
- No custom ECS implementation; EnTT used directly
- Components are POD structs (no methods), accessed via `registry.get<ComponentType>(entity)`
- Systems query via `registry.view<>()` and `registry.group<>()` for batching
- Execution phases managed by Application (UpdatePhase enum, systems ordered per phase)

## Data Serialization

**JSON (Editor & Debug):**
- Library: nlohmann/json v3.11.3 (header-only)
- Used by: Editor debug harness command protocol (see `src/editor/debug/EditorCommander.h`)
- Format: Line-delimited JSON commands sent over Unix socket
- Example command: `{"id": 1, "cmd": "inspect.entities", "args": {}}`

**Scene Files (Binary + Metadata):**
- Format: Custom `.scene` files (binary with embedded LevelDef structs + metadata)
- Loading: `src/game/level/LevelLoader.h` deserializes from file
- Serialization: Scene graph roundtrip via `src/editor/scene/EditorSceneSerializer.cpp`
- No external serialization library (custom binary format optimized for fast load)

## Logging Integration

**spdlog v1.x:**
- Header-only logger (fast, fmt-backed)
- Used throughout engine for subsystem logging
- Category sinks allow filtering (rendering, AI, physics, audio logs separately)
- Example: `src/engine/core/` uses spdlog for Application events

## Math Integration

**GLM 1.0.3:**
- Header-only math library
- Provides: `glm::vec3`, `glm::mat4`, `glm::quat`, common operations
- GLSL syntax mirrors shader-side math (facilitates CPU/GPU sync)
- No wrapper layer; used directly in all game/engine code
- Dependency: `target_link_libraries(... glm::glm)` in CMakeLists.txt

## Build Configuration Integration

**CMake Build System:**
- FetchContent for remote dependencies (GLAD, EnTT, Jolt, ImGui, tinygltf, Assimp, nlohmann_json)
- System package discovery via `find_package()` (GLFW, GLM, spdlog, OpenAL)
- Custom `cmake/DesktopApp.cmake` macro applies platform-specific link flags and framework includes
- Test registration via `cmake/TestSupport.cmake` (standalone executables, no external test framework)

**Python GLAD Generation:**
- Requires Python 3 + jinja2 in isolated `.venv`
- Configure step: `cmake .. -DPython_Executable=.venv/bin/python3`
- GLAD C/C++ bindings generated locally for OpenGL 4.1 Core Profile

## Performance Monitoring

**Profiling & Metrics:**
- ImGui debug overlay displays FPS, frame time (ms), draw call count
- No external profiling library; metrics captured locally in rendering passes
- spdlog used for frame-level timing logs

## Platform-Specific Integrations

**macOS:**
- OpenGL framework (deprecated but required for OpenGL 4.1 Core support)
- Cocoa framework for window management (GLFW abstraction)
- IOKit framework for device input polling
- Homebrew package paths detected at CMake time (keg-only OpenAL Soft, zlib for Assimp)
- GL_SILENCE_DEPRECATION compile flag (line 13 in DesktopApp.cmake)

**Windows/Linux:**
- OpenGL 4.1 Core supported via system drivers
- No platform-specific GUI frameworks; GLFW + OpenGL + Cocoa abstractions handle all

---

*Integration audit: 2026-04-05*
