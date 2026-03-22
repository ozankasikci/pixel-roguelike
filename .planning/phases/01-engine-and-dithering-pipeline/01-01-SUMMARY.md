---
phase: 01-engine-and-dithering-pipeline
plan: 01
subsystem: renderer
tags: [opengl, glfw, glad, glm, spdlog, cmake, glsl, dithering, framebuffer]

# Dependency graph
requires: []
provides:
  - CMake build system with GLAD 2 FetchContent for OpenGL 4.1 Core Profile
  - GLFW window wrapper with OpenGL 4.1 Core Profile context (macOS compatible)
  - Offscreen FBO renderer (GL_RGB16F color + GL_DEPTH_COMPONENT24 depth, GL_NEAREST filter)
  - Bayer 8x8 dither post-process pass with sphere-map world-space anchoring
  - Scene shader (Blinn-Phong-ready with MVP uniforms)
  - Mesh system with cube and plane factory methods
  - Full 1-bit rendering pipeline: 480p FBO scene pass -> Bayer dither -> display output
affects:
  - 01-02-lighting-and-imgui
  - All subsequent render phases

# Tech tracking
tech-stack:
  added:
    - GLAD 2 v2.0.8 (via CMake FetchContent, gl:core=4.1)
    - GLFW 3.4 (Homebrew arm64)
    - GLM 1.0.3 (Homebrew)
    - spdlog 1.17.0 (Homebrew)
    - jinja2 (Python venv, required for GLAD 2 code generation)
  patterns:
    - Offscreen FBO + fullscreen quad dither post-process
    - Sphere-map world-space dither anchoring via inverse view matrix
    - GL_RGB16F linear color space (no sRGB gamma) for dither threshold comparison
    - GLFW_OPENGL_FORWARD_COMPAT required on macOS for Core Profile context

key-files:
  created:
    - CMakeLists.txt
    - src/main.cpp
    - src/engine/Window.h
    - src/engine/Window.cpp
    - src/renderer/Shader.h
    - src/renderer/Shader.cpp
    - src/renderer/Framebuffer.h
    - src/renderer/Framebuffer.cpp
    - src/renderer/DitherPass.h
    - src/renderer/DitherPass.cpp
    - src/renderer/Mesh.h
    - src/renderer/Mesh.cpp
    - assets/shaders/scene.vert
    - assets/shaders/scene.frag
    - assets/shaders/dither.vert
    - assets/shaders/dither.frag
    - .gitignore
  modified: []

key-decisions:
  - "GLAD 2 requires jinja2 Python package: use python3 -m venv .venv && .venv/bin/pip install jinja2, then cmake with -DPython_EXECUTABLE=.venv/bin/python3"
  - "CMakeLists.txt must declare LANGUAGES C CXX (not just CXX) because GLAD 2 generates a C static library"
  - "GL_NEAREST filter on FBO color texture is critical: prevents interpolation artifacts in the dither pass"
  - "Sphere-map world-space anchoring: uInverseView (pure rotation, no translation) passed per frame to dither.frag"
  - "Linear color space (GL_RGB16F) throughout: dither threshold comparison in linear space produces smoother gradients"

patterns-established:
  - "Pattern: FBO bind -> scene draw -> FBO unbind -> dither pass apply -> swap buffers"
  - "Pattern: glm::inverse(viewMatrix) per frame for world-space dither stability"
  - "Pattern: Shader uniform setters check location != -1 silently (no crash on unused uniforms)"
  - "Pattern: Mesh factory methods createCube()/createPlane() return by value (move-constructible)"

requirements-completed: [RNDR-01, RNDR-02, RNDR-03, RNDR-04]

# Metrics
duration: 5min
completed: 2026-03-23
---

# Phase 01 Plan 01: Engine and Dithering Pipeline Summary

**OpenGL 4.1 Core Profile C++ engine with 8x8 Bayer dither post-process using sphere-map world-space anchoring at 480p internal resolution upscaled with nearest-neighbor**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-03-22T23:21:49Z
- **Completed:** 2026-03-23T00:16:00Z
- **Tasks:** 2 of 2
- **Files modified:** 17

## Accomplishments

- Complete C++ build system using CMake FetchContent to pull GLAD 2 v2.0.8 targeting OpenGL 4.1 Core Profile on macOS arm64
- Full 1-bit rendering pipeline: 3D scene rendered to 854x480 GL_RGB16F FBO, then Bayer 8x8 dither pass to pure black/white at display resolution
- World-space dither anchoring via sphere-map technique: camera orbit does not cause dither pattern to swim or crawl

## Task Commits

Each task was committed atomically:

1. **Task 1: CMake build system, project structure, and GLFW window with OpenGL 4.1** - `7c27529` (feat)
2. **Task 2: FBO, shader system, dither post-process with world-space anchoring, and test geometry** - `e4cb424` (feat)

## Files Created/Modified

- `CMakeLists.txt` - FetchContent GLAD 2, find_package GLFW/GLM/spdlog, macOS framework links
- `src/main.cpp` - Full pipeline: 480p FBO scene pass -> dither pass at display resolution, orbiting camera
- `src/engine/Window.h` / `.cpp` - GLFW window with OpenGL 4.1 Core Profile, gladLoadGL (GLAD 2 API)
- `src/renderer/Shader.h` / `.cpp` - GLSL loader/compiler with uniform setters
- `src/renderer/Framebuffer.h` / `.cpp` - GL_RGB16F FBO with GL_DEPTH_COMPONENT24 RBO, GL_NEAREST
- `src/renderer/DitherPass.h` / `.cpp` - Fullscreen quad VAO, applies dither.vert/frag
- `src/renderer/Mesh.h` / `.cpp` - Position+normal VAO/VBO/EBO, createCube() and createPlane()
- `assets/shaders/scene.vert` / `.frag` - MVP vertex transform, simple directional diffuse lighting
- `assets/shaders/dither.vert` / `.frag` - Fullscreen quad, 8x8 Bayer matrix, sphereUV(), step() 1-bit output
- `.gitignore` - Excludes build/ and .venv/

## Decisions Made

- **GLAD 2 needs C language**: Had to add `LANGUAGES C CXX` to CMakeLists.txt project() — GLAD generates a C static library
- **Python jinja2 in venv**: GLAD 2's CMake code generation requires jinja2; installed in `.venv` per CLAUDE.md (no system pip)
- **GL_NEAREST on FBO texture**: Critical — prevents interpolation artifacts in the dither pass input
- **uInverseView uniform**: Pure rotation inverse-view matrix (not MVP) anchors the sphere-map dither pattern to world space

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added C language to CMake project declaration**
- **Found during:** Task 1 (CMake configuration)
- **Issue:** `project(roguelike LANGUAGES CXX)` caused CMake error: "CMAKE_C_COMPILE_OBJECT not set" because GLAD 2 generates a C static library requiring C compiler support
- **Fix:** Changed to `project(roguelike LANGUAGES C CXX)`
- **Files modified:** CMakeLists.txt
- **Verification:** CMake configuration succeeds, GLAD 2 compiles and links correctly
- **Committed in:** `7c27529` (Task 1 commit)

**2. [Rule 3 - Blocking] Created Python venv and installed jinja2 for GLAD 2 generation**
- **Found during:** Task 1 (first cmake --build)
- **Issue:** GLAD 2's Python code generator requires jinja2 (`ModuleNotFoundError: No module named 'jinja2'`); system pip install blocked by PEP 668
- **Fix:** Created `.venv` (python3 -m venv .venv), installed jinja2, configured cmake with `-DPython_EXECUTABLE=.venv/bin/python3`
- **Files modified:** .gitignore (added .venv/)
- **Verification:** GLAD 2 code generation succeeded, libglad_gl.a compiled
- **Committed in:** `7c27529` (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both fixes required for the build to succeed on macOS. No scope creep. Documented GLAD 2 + jinja2 requirement in CMakeLists.txt comment for future contributors.

## Issues Encountered

- GLAD 2 has an undocumented Python jinja2 dependency that only manifests at build time (not configure time). This is a common gotcha for macOS users with PEP 668-enforced Python. The venv workaround is clean and follows CLAUDE.md requirements.

## User Setup Required

To build for the first time on a fresh machine:
```bash
# Install Homebrew dependencies
brew install cmake glfw glm spdlog

# Create Python venv for GLAD 2 code generation
python3 -m venv .venv
.venv/bin/pip install jinja2

# Configure and build
mkdir build && cd build
cmake .. -DPython_EXECUTABLE=../.venv/bin/python3
cmake --build .
```

## Next Phase Readiness

- Full 1-bit dither pipeline is operational and the visual identity is locked
- Framebuffer, Shader, Mesh, and DitherPass classes provide clean interfaces for Phase 01-02
- Ready to add ImGui debug overlay (dither parameter tuning), Blinn-Phong point lights, and WASD camera

---
*Phase: 01-engine-and-dithering-pipeline*
*Completed: 2026-03-23*
