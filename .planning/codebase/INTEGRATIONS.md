# External Integrations

**Analysis Date:** 2026-04-01

## Graphics API Integration

**OpenGL 4.1 Core Profile:**
- Loader: GLAD 2 (`v2.0.8`), generated as `glad_gl` static library configured for `gl:core=4.1`. Requires Python + jinja2 at CMake configure time.
- Loader header: `<glad/gl.h>` — included in `Shader.h`, `Texture2D.h`, `ShadowMap.h`, `Framebuffer.h`, all OpenGL-touching files.
- Platform: macOS OpenGL silences deprecation warnings via `GL_SILENCE_DEPRECATION` (set in `cmake/DesktopApp.cmake` for all app targets).
- Context creation: GLFW (`glfw3 3.4`) via `Window.cpp` (`src/engine/core/Window.cpp`). `GLFWwindow*` stored and exposed as `Window::handle()`.
- macOS link flags: `-framework Cocoa`, `-framework OpenGL`, `-framework IOKit` injected by `configure_desktop_app()`.

**Shader compilation pipeline:**
- All shaders use `#version 410 core` (macOS ceiling).
- Shaders loaded from disk at runtime via `Shader::readFile()` in `src/engine/rendering/core/Shader.cpp`.
- Compilation and linking done via OpenGL driver: `glCreateShader`, `glCompileShader`, `glCreateProgram`, `glLinkProgram`.
- No offline shader compilation or SPIR-V; errors reported at runtime via spdlog.
- Three shader constructor overloads in `Shader`: `(vert, frag)`, `(vert, geom, frag)`. Geometry shader used by CSM depth pass (`csm_depth.geom`).
- Engine shaders location: `assets/shaders/engine/` — composite, stylize, shadow_depth, csm_depth, bloom_downsample, bloom_upsample, ssao, ssao_blur.
- Game shaders location: `assets/shaders/game/` — scene.vert / scene.frag (PBR-like; 32 lights, LTC area lights, shadow maps, procedural textures).

**Framebuffer objects:**
- `Framebuffer` class in `src/engine/rendering/core/Framebuffer.h` — RAII wrapper for FBO + color/depth attachments.
- Scene rendered to an off-screen FBO (`sceneFBO_` in `SceneRenderPipeline`), then post-processed and composited.
- `SceneRenderPipeline::render()` in `src/engine/rendering/SceneRenderPipeline.cpp` orchestrates the full frame: shadow pass → CSM → scene pass → bloom → SSAO → composite → stylize.

## Physics Engine Integration

**Jolt Physics (`v5.4.0`):**
- Integrated behind a pimpl boundary in `src/engine/physics/PhysicsSystem.cpp` / `PhysicsSystem.h`.
- Jolt headers used: `<Jolt/Jolt.h>`, `RegisterTypes.h`, `Factory.h`, `TempAllocator.h`, `JobSystemThreadPool.h`, `PhysicsSettings.h`, `PhysicsSystem.h`, `BoxShape.h`, `CapsuleShape.h`, `CylinderShape.h`, `RotatedTranslatedShape.h`, `BodyCreationSettings.h`, `BodyActivationListener.h`, `CharacterVirtual.h`.
- Jolt is linked privately into `engine_physics` — callers depend only on `PhysicsSystem.h` (no Jolt headers exposed).
- API exposed to gameplay: `setCharacterVelocity()`, `updateCharacter()`, `getCharacterPosition()`, `setCharacterPosition()`, `getCharacterGroundState()` (returns `GroundState` enum: `OnGround`, `OnSteepGround`, `InAir`).
- Static colliders spawned from `StaticColliderComponent` via `PhysicsSystem::init(entt::registry&)`.
- CMake: all sample/viewer/test targets disabled before `FetchContent_MakeAvailable(JoltPhysics)`.

## Audio System Integration

**OpenAL Soft (`v1.25.1`):**
- Integrated in `src/engine/audio/AudioSystem.cpp` with pimpl pattern.
- Headers: `<AL/al.h>`, `<AL/alc.h>`.
- macOS: Homebrew keg-only path forced via CMake (`/opt/homebrew/opt/openal-soft/`) to avoid macOS system framework.
- SFX: WAV files loaded via custom 44-byte header parser in `AudioSystem.cpp`. Preloaded into OpenAL buffers, played positionally with `alSource3f(AL_POSITION)`.
- Music streaming: OGG Vorbis via `external/stb_vorbis.c`. Streaming with double-buffering into OpenAL source.
- Ambient streaming: Same OGG/stb_vorbis pipeline as music but separate source.
- Listener transform updated each frame by `AudioListenerSystem` (`src/game/systems/AudioListenerSystem.cpp`) calling `AudioSystem::setListenerTransform()`.
- Volume categories: master, SFX, music, ambient (0.0–1.0 floats).

**OGG Vorbis (stb_vorbis):**
- `external/stb_vorbis.c` compiled as C source (`set_source_files_properties(... LANGUAGE C)`) and linked into `engine_audio`.
- Used for music and ambient streaming in `AudioSystem`.

## Asset Loading Pipeline

**Model loading:**
- `ModelLoader` (`src/engine/rendering/assets/ModelLoader.h`) is the unified entry point. Routes by file extension.
- GLB/glTF: loaded via `GltfLoader` (`src/engine/rendering/assets/GltfLoader.cpp`) using tinygltf (`v2.9.3`). tinygltf stb_image integration disabled (`TINYGLTF_NO_STB_IMAGE`, `TINYGLTF_NO_STB_IMAGE_WRITE`) since textures are loaded separately.
- FBX and legacy formats: loaded via `AssimpLoader` (`src/engine/rendering/assets/AssimpLoader.cpp`) using Assimp (`v6.0.4`). Post-process flags include triangulate, join identical vertices, smooth normals, calc tangents, pre-transform vertices.
- Submesh loading: `ModelLoader::loadRawMulti()` groups FBX meshes by material name; glTF returns single merged mesh.
- Asset cache: `AssetCache` (`src/engine/rendering/assets/AssetCache.h`) caches processed mesh data to disk using FNV-1a 64-bit hash of source file contents. Cache invalidated on source file change. Texture cache also supported. Cache root from `AssetCache::cacheRoot()`.
- Mesh discovery: `ModelLoader::discoverProjectAssets()` walks the assets directory for `.glb` and `.fbx` files.
- Supported mesh formats in `assets/meshes/`: `.glb` (arch, pillar, hand, dagger, gothic door) and `.fbx` (country_house, doors, wood_door).

**Texture loading:**
- `Texture2D` (`src/engine/rendering/assets/Texture2D.cpp`) — PNG/JPG via `stb_image` (pulled in through tinygltf's STB_IMAGE_IMPLEMENTATION).
- `TextureCube` (`src/engine/rendering/assets/TextureCube.cpp`) — cubemap loading for skyboxes (cubemap PNGs in `assets/skies/`).
- Texture formats used: RGBA8, R8 (for AO/roughness).
- Material textures: albedo map, normal map, AO map — paths stored in `.material` asset files.

**Material asset format:**
- Custom key-value text format, parsed by `ContentRegistry` from `assets/materials/*.material` files.
- Fields: `id`, `base_color`, `uv_mode`, `uv_scale`, `normal_strength`, `roughness_scale`, `roughness_bias`, `metalness`, `ao_strength`, `albedo_map`, `normal_map`, `ao_map`, `normal_map_flip_y`.
- Example: `assets/materials/wood_door_1.material`.
- `ContentRegistry` supports hot-reload polling every 500ms (editor only) via `pollMaterialHotReload()`.

**Scene asset format:**
- Custom text format (one entity per line). Fields: mesh ID, position, scale, rotation, material, tint, node ID.
- Example: `assets/scenes/institutional_room.scene`, `cathedral.scene`, `silos_cloister.scene`.
- Loaded by `LevelLoader` (`src/game/level/LevelLoader.cpp`) into `LevelDef` structs, then spawned into ECS by `LevelBuilder`.
- `project.cfg` tracks `last_scene` key for editor auto-open.

**Screenshot writing:**
- `Screenshot` (`src/engine/ui/Screenshot.cpp`) — uses `external/stb_image_write.h` to write PNG files.

## LTC Area Light Lookup Tables

**Heitz et al. LTC (Linearly Transformed Cosines):**
- `LtcData` (`src/engine/rendering/lighting/LtcData.cpp`) — uploads two 64x64 RGBA32F GPU textures encoding the LTC inverse transform matrix and GGX amplitude/Fresnel.
- Data baked in from `selfshadow/ltc_code` lookup tables (embedded in source, not loaded from disk).
- Used by the scene shader for real-time area light shading (AreaRect and Tube light types).

## Editor Gizmo Integration

**ImGuizmo (vendored):**
- `external/ImGuizmo/ImGuizmo.cpp` + `ImGuizmo.h` — 3D transform gizmos (translate/rotate/scale) rendered over the editor viewport.
- Compiled directly into the `editor` static library (`src/editor/CMakeLists.txt`).
- Used in `src/editor/viewport/EditorViewportController.cpp` via `ImGuizmo::Manipulate()`.
- Operates within Dear ImGui's draw list; requires `ImGuizmo::SetOrthographic`, `SetDrawlist`, `SetRect` each frame.

## Dear ImGui Integration

**Dear ImGui (`v1.92.6-docking`):**
- Uses docking branch for the level editor's multi-panel dock layout.
- Backends: `imgui_impl_glfw.cpp` + `imgui_impl_opengl3.cpp` compiled into the `imgui` static target.
- `ImGuiLayer` (`src/engine/ui/ImGuiLayer.cpp`) wraps `ImGui_ImplGlfw_InitForOpenGL`, `ImGui_ImplOpenGL3_Init`, `beginFrame()`, `endFrame()`.
- Runtime game uses ImGui only for `DebugParams` overlay (can be stripped for release).
- Editor uses ImGui for full dock layout: inspector, outliner, environment panel, asset browser, viewport.
- Font loading: TTF files from `assets/fonts/editor/` loaded into ImGui atlas at runtime. Font preset system in `ImGuiLayer` (JetBrainsMono, Inter, Roboto, plus system fonts).

## CI/CD & Deployment

**CI Pipeline:**
- No CI configuration found (no `.github/workflows/`). Only a PR template at `.github/PULL_REQUEST_TEMPLATE.md`.

**Hosting:**
- Desktop application; no cloud deployment. Executables built locally.

## File Storage

**Local filesystem only:**
- All assets (meshes, textures, shaders, scenes, materials, skies, fonts) loaded from relative `assets/` path at runtime.
- No remote asset delivery, CDN, or database.
- Asset cache: disk-local, in a subdirectory resolved by `AssetCache::cacheRoot()`.

## Authentication & External APIs

- None. This is a self-contained desktop game engine. No network calls, no auth, no web APIs, no telemetry.

---

*Integration audit: 2026-04-01*
