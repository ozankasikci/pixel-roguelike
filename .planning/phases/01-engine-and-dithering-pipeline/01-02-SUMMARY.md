---
phase: 01-engine-and-dithering-pipeline
plan: 02
subsystem: renderer
tags: [opengl, glsl, blinn-phong, point-lights, imgui, glfw, cmake, dithering, cathedral]

# Dependency graph
requires:
  - phase: 01-engine-and-dithering-pipeline
    plan: 01
    provides: FBO pipeline, Shader/Mesh/DitherPass API, 1-bit dither post-process

provides:
  - Forward Blinn-Phong renderer (Renderer class) with PointLight array and drawScene()
  - Cathedral test scene with 6 pillars, 2 arches, floor, ceiling, back wall, and 5 point lights
  - Quartic attenuation GLSL (pow(dist/radius, 4.0)) eliminating hard ring artifacts in dithered output
  - Dear ImGui debug overlay with Dither, Camera, Performance, and Lights sections
  - Runtime FBO resize (480p/540p/720p) via ImGui resolution combo
  - WASD + arrow key camera movement with configurable FOV and speed

affects:
  - 02-procedural-generation (uses Renderer/CathedralScene patterns for dungeon rooms)
  - All subsequent render phases (Renderer.drawScene is the main scene submission API)

# Tech tracking
tech-stack:
  added:
    - Dear ImGui v1.92.6 (via CMake FetchContent, GLFW + OpenGL3 backends)
  patterns:
    - Renderer owns shader + light uniforms; caller provides RenderObject list and PointLight list
    - Quartic attenuation clamp(1 - pow(dist/radius, 4)^2 avoids dither ring artifacts (Pitfall 6)
    - ImGuiLayer init/beginFrame/endFrame wraps NewFrame/Render always-paired requirement (Pitfall 5)
    - ImGui::GetIO().WantCaptureMouse gates mouse-look vs UI interaction

key-files:
  created:
    - src/renderer/Renderer.h
    - src/renderer/Renderer.cpp
    - src/game/CathedralScene.h
    - src/game/CathedralScene.cpp
    - src/engine/ImGuiLayer.h
    - src/engine/ImGuiLayer.cpp
  modified:
    - assets/shaders/scene.frag
    - src/main.cpp
    - CMakeLists.txt
    - src/renderer/DitherPass.h
    - src/renderer/DitherPass.cpp

key-decisions:
  - "DitherPass::apply() updated to accept patternScale float parameter (was hardcoded 256.0) so ImGui slider can tune it at runtime"
  - "Dear ImGui v1.92.6 integrated via CMake FetchContent to avoid shipping pre-compiled binaries"
  - "ImGui::GetIO().WantCaptureMouse used to switch between camera mouse-look and ImGui interaction without separate toggle key"
  - "Quartic attenuation formula (Pitfall 6 from RESEARCH.md) eliminates concentric ring artifacts in Bayer dither output"

patterns-established:
  - "Pattern: Renderer::drawScene() sets all uniforms (MVP, lights, camera pos) then iterates RenderObject list — single-call scene submission"
  - "Pattern: ImGuiLayer owns all ImGui lifecycle; main.cpp only calls beginFrame/endFrame and renderOverlay()"
  - "Pattern: DebugParams struct as single truth for all tunable runtime parameters passed between ImGui and render systems"

requirements-completed: [RNDR-05]

# Metrics
duration: ~15min
completed: 2026-03-23
---

# Phase 01 Plan 02: Lighting and ImGui Debug Overlay Summary

**Blinn-Phong point lighting with quartic attenuation on a cathedral test scene, rendered 1-bit dithered at 480p, with a Dear ImGui overlay for runtime tuning of dither threshold, pattern scale, resolution, camera, and per-light parameters**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-03-23T02:31:00Z
- **Completed:** 2026-03-23T02:35:00Z
- **Tasks:** 3 of 3 (including checkpoint verification)
- **Files modified:** 11

## Accomplishments

- Forward Blinn-Phong renderer with 5-light cathedral scene replacing the test cube from Plan 01
- Quartic attenuation (`clamp(1 - pow(dist/radius, 4))^2`) eliminates hard ring artifacts in 1-bit dithered output (RNDR-05)
- Dear ImGui debug overlay with collapsible sections for Dither, Camera, Performance, and Lights — all verified functional at runtime
- Runtime FBO resize (480p / 540p / 720p) via resolution combo, DitherPass::apply() now accepts patternScale as a parameter
- Visual verification passed: pure black/white output, no dither swimming on camera movement, chunky 480p cells visible, point lights create brightness gradients, ImGui controls all respond correctly

## Task Commits

Each task was committed atomically:

1. **Task 1: Forward Blinn-Phong lighting and cathedral test scene** - `2b8d1dc` (feat)
2. **Task 2: Dear ImGui debug overlay with runtime parameter tuning** - `bd1891d` (feat)
3. **Task 3: Visual verification checkpoint** - approved (no code changes, human-verified)

## Files Created/Modified

- `src/renderer/Renderer.h` / `.cpp` - Renderer class with PointLight/RenderObject structs and drawScene()
- `src/game/CathedralScene.h` / `.cpp` - Cathedral geometry (6 pillars, 2 arches, floor, ceiling, back wall) and 5 point lights
- `assets/shaders/scene.frag` - Blinn-Phong with quartic attenuation replacing simple directional diffuse
- `src/engine/ImGuiLayer.h` / `.cpp` - ImGuiLayer lifecycle + DebugParams struct + renderOverlay() with 4 collapsible sections
- `src/main.cpp` - CathedralScene integration, WASD/arrow camera, ImGui wiring, resolution FBO resize, debugParams pipeline
- `CMakeLists.txt` - Added Dear ImGui via FetchContent, added Renderer.cpp, CathedralScene.cpp, ImGuiLayer.cpp
- `src/renderer/DitherPass.h` / `.cpp` - Updated apply() to accept patternScale float parameter

## Decisions Made

- **DitherPass::apply() patternScale parameter**: The plan specified `debugParams.patternScale` being passed to `ditherPass.apply()`, which required updating the DitherPass API. Added `float patternScale = 256.0f` as a parameter with the same default — no breaking change for existing call site.
- **Dear ImGui FetchContent**: Avoids vendoring pre-built binaries; FetchContent pulls v1.92.6 source at configure time and builds it as a static library alongside the project.
- **WantCaptureMouse for input routing**: When ImGui windows are hovered, mouse-look is suspended; clicking outside ImGui panels re-engages mouse capture. No explicit toggle key needed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated DitherPass::apply() signature to accept patternScale**
- **Found during:** Task 2 (ImGui wiring in main.cpp)
- **Issue:** Plan specified `ditherPass.apply(thresholdBias, patternScale)` but DitherPass::apply() had no patternScale parameter — it was hardcoded to 256.0 in the dither shader uniform
- **Fix:** Added `float patternScale = 256.0f` parameter to DitherPass::apply() and passed it through to the `uPatternScale` uniform in dither.frag
- **Files modified:** src/renderer/DitherPass.h, src/renderer/DitherPass.cpp
- **Verification:** Changing Pattern Scale slider in ImGui visibly changes dither tile frequency
- **Committed in:** `bd1891d` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Required to wire the patternScale ImGui slider to the actual dither pass. No scope creep.

## Issues Encountered

None beyond the auto-fixed deviation above.

## User Setup Required

None — no new external service configuration required. Dear ImGui is pulled via FetchContent at build time, no separate installation needed. See Plan 01-01 SUMMARY for the initial Python venv / jinja2 / GLAD 2 setup required on a fresh machine.

## Next Phase Readiness

- Complete Phase 01 success criteria met: 1-bit dither pipeline, world-space anchoring, point light density gradients, ImGui runtime tuning
- Renderer.drawScene() is the scene submission API for all future phases
- CathedralScene geometry serves as the visual reference target for Phase 02 procedural generation
- No blockers — Phase 02 can begin

---
*Phase: 01-engine-and-dithering-pipeline*
*Completed: 2026-03-23*
