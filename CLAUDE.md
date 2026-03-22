<!-- GSD:project-start source:PROJECT.md -->
## Project

**3D Roguelike**

A first-person 3D roguelike with a distinctive 1-bit dithered black-and-white visual style, built on a custom C++ engine. The player explores gothic cathedral-like environments, fighting enemies with melee and ranged weapons, progressing through handcrafted levels with escalating difficulty and boss encounters.

**Core Value:** The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable. If the look doesn't land, nothing else matters.

### Constraints

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: To be determined by research (OpenGL vs Vulkan tradeoff)
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
| OpenGL 4.6 Core Profile | 4.6 | Graphics API | Right choice for this project — custom 1-bit post-process shader is a simple fullscreen quad blit; the complexity overhead of Vulkan would dominate the schedule without meaningfully improving the dithered aesthetic; OpenGL 4.6 is available on all desktop targets; deprecated on macOS means Metal support would need attention later, but macOS-first is not a stated requirement |
| GLFW | 3.4 (released Feb 2024) | Window creation, OpenGL context, input | Focused and minimal — exactly what a custom engine needs; callback-based input maps cleanly to an event bus architecture; 12ms window creation vs SDL's 45ms; no audio/networking bloat; 3.4 adds runtime platform selection and Wayland support |
| GLAD 2 | v2.0.8 (released Sep 2025) | OpenGL function loader | Modern replacement for GLEW; generates only the extension set you request; supports OpenGL Core profile natively without the `glewExperimental` workaround GLEW needs; GLAD 2 adds Vulkan/EGL support if the API is ever extended |
| GLM | 1.0.3 (released Dec 2025) | Math: vectors, matrices, quaternions | Header-only; syntax mirrors GLSL exactly, which matters when writing shader-side math alongside CPU-side transforms; the only maintained C++ math lib with full GLSL spec coverage; 1.0.x branch supports C++17/20 |
| EnTT | v3.16.0 (released Nov 2025) | Entity-Component System (ECS) | Used in Minecraft; header-only; archetype-sparse-set hybrid delivers cache-friendly iteration; component queries are composable at compile time; forces clean separation between game state and rendering that makes the dithering post-pass trivial to layer on top |
### Post-Processing / Rendering Support
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| GLSL (via OpenGL) | 4.60 | Shader language | The 1-bit Bayer dithering shader is a fragment shader operating on a fullscreen texture; GLSL is the only choice when targeting OpenGL; see shader note below |
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
# https://glad.dav1d.de/ — select OpenGL 4.6, Core profile, generate C/C++
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
| OpenGL 4.6 | Vulkan | If targeting platforms where OpenGL is fully deprecated (macOS) and Metal bridging is unacceptable, or if you need GPU-driven rendering for thousands of draw calls — neither applies here |
| OpenGL 4.6 | WebGPU (via wgpu/Dawn) | If a web/browser deployment target is added later |
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
| Assimp | Massive dependency for loading 3D models; 1M+ lines of code; build is fragile; for a gothic dungeon game with handcrafted levels, you control the asset pipeline — use glTF + tinygltf instead | tinygltf (header-only glTF 2.0 loader) |
| Unity / Unreal / Godot | Explicitly out of scope per project requirements; the dithering post-pass needs direct FBO control that is difficult or impossible to achieve in managed pipelines | Custom C++ + OpenGL (as specified) |
| Bullet3 | Last meaningful update was 2022; development is effectively frozen; Jolt Physics is its modern successor | Jolt Physics v5 |
| OpenGL compatibility profile | Allows deprecated immediate-mode calls to leak in; makes porting to Vulkan harder; produces driver-specific behavior differences | OpenGL 4.6 Core Profile only |
| Boost | Enormous dependency for utility functions that C++20 STL now provides; adds significant compile time | C++20 STL (`std::span`, `std::ranges`, `std::format`) |
## Stack Patterns by Variant
- Replace OpenGL 4.6 with Metal (via the `metal-cpp` header-only wrapper from Apple)
- Or use MoltenVK to run Vulkan on Metal
- OpenGL on macOS is deprecated at 4.1 — Apple has not updated it since 2018
- Replace OpenGL + GLFW with WebGPU (Emscripten's built-in WebGPU support)
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

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
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
