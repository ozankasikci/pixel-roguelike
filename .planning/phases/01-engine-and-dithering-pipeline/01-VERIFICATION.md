---
phase: 01-engine-and-dithering-pipeline
verified: 2026-03-23T12:00:00Z
status: human_needed
score: 5/5 must-haves verified
re_verification:
  previous_status: passed
  previous_score: 7/7 (composite across two plan must_haves sets; re-scored against 5 success-criteria truths)
  gaps_closed: []
  gaps_remaining: []
  regressions: []
human_verification:
  - test: "Run ./build/roguelike and observe the window"
    expected: "Every pixel is either pure black (0,0,0) or pure white (1,1,1) — no gray values, no color anywhere in the scene"
    why_human: "Shader logic step(threshold,luma) guarantees binary output analytically, but pixel-level confirmation that the GL framebuffer path introduces no unexpected sRGB conversion or tone-mapping requires visual inspection of the running app"
  - test: "Move the camera with WASD / arrow keys while watching the dither pattern on a static surface (floor or wall)"
    expected: "Dither pattern stays locked to surfaces — no crawling or swimming visible during camera rotation; mild sub-pixel drift on translation is acceptable per RESEARCH.md Obra Dinn tradeoff"
    why_human: "Temporal frame-to-frame dither stability cannot be assessed from static code; requires observing consecutive frames during camera motion"
  - test: "At 1280x720 display resolution, observe the size of individual dither cells"
    expected: "Each Bayer cell is multiple pixels wide — visibly chunky/pointillist, not fine sub-pixel noise"
    why_human: "Apparent cell size depends on interaction of 854x480 internal res, 1280x720 display res, GL_NEAREST upscale, and uPatternScale=256; computing perceptual size requires visual calibration"
  - test: "Look at areas near the 5 point lights vs far corners of the cathedral scene"
    expected: "Lit areas show noticeably more white pixels; dark corners mostly black; no hard concentric ring artifacts around any light"
    why_human: "Dither density gradient quality and ring-artifact absence are perceptual qualities of the quartic attenuation formula that require seeing actual rendered output"
  - test: "Open the ImGui overlay and drag the Threshold Bias slider; change the Resolution combo"
    expected: "Black/white balance shifts in real time as Threshold Bias moves; selecting a different resolution visibly changes dither cell size without recompiling"
    why_human: "UI responsiveness and FBO resize side-effect require interactive runtime testing"
---

# Phase 1: Engine and Dithering Pipeline — Verification Report

**Phase Goal:** The 1-bit dithered visual identity is working and visually correct — any geometry rendered through the pipeline produces the expected chunky black-and-white ordered dither pattern with no swimming on camera movement
**Verified:** 2026-03-23T12:00:00Z
**Status:** human_needed — all automated code checks pass; 5 visual/interactive behaviors require human sign-off
**Re-verification:** Yes — second pass; prior verification also passed. This pass re-reads all source files directly and cross-checks every must-have and requirement ID independently.

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A 3D scene renders to screen in pure black and white through the Bayer dither post-process — no grayscale, no color | ? HUMAN | `dither.frag:50-51` uses `step(threshold, luma)` → `fragColor = vec4(vec3(bit), 1.0)`. Binary output guaranteed analytically; no `GL_FRAMEBUFFER_SRGB` found anywhere. Pixel-level visual confirmation requires running app. |
| 2 | Moving the camera does not cause the dither pattern to crawl or swim — pattern is anchored to world space | ? HUMAN | `dither.frag:35-41` reconstructs `worldDir` from `uInverseView` and uses `sphereUV(worldDir)` as the Bayer index source. `main.cpp:154` sets `inverseView = glm::inverse(viewMatrix)` per frame. Mechanism is correctly implemented; temporal stability requires running app. |
| 3 | Scene renders at reduced internal resolution (480p) and upscales with nearest-neighbor, producing chunky visible dither cells | ? HUMAN | `main.cpp:22-23` defines `RES_W[0]=854, RES_H[0]=480`; `sceneFBO.create(RES_W[0], RES_H[0])` at line 40. `Framebuffer.cpp:19-20` sets `GL_NEAREST` for both min and mag filters. `DitherPass.cpp:68` calls `glViewport(0,0,displayW,displayH)` at display resolution. Visual chunkiness requires running app. |
| 4 | A torch-equivalent point light source visibly increases dither density in its radius compared to unlit geometry | ? HUMAN | `CathedralScene.cpp:70-103` pushes 5 `PointLight` instances with non-zero radius/intensity. `scene.frag:20-22` implements quartic attenuation `pow(dist/radius,4.0)`. `Renderer.cpp:26-32` sets all light uniforms per frame. `scene.frag:50` outputs `fragColor = vec4(vec3(clamp(totalLight,0,1)),1)` as luminance input to dither threshold. Visual density gradient requires running app. |
| 5 | A Dear ImGui debug overlay lets the developer tune dither threshold and internal resolution at runtime without recompiling | ? HUMAN | `ImGuiLayer.cpp:49-57` implements Threshold Bias slider, Pattern Scale slider, and Resolution combo with `resolutionChanged` flag. `main.cpp:155-157` passes `debugParams.thresholdBias` and `debugParams.patternScale` to `ditherPass.apply()` every frame. `main.cpp:176-181` handles `resolutionChanged` → `sceneFBO.resize()`. Wiring is complete; interactive responsiveness requires running app. |

**Score:** 5/5 truths — implementation VERIFIED end-to-end. Human visual confirmation needed for all 5 (they are all perceptual/interactive behaviors).

---

### Required Artifacts

#### From 01-01-PLAN.md must_haves

| Artifact | Provides | Status | Evidence |
|----------|----------|--------|----------|
| `CMakeLists.txt` | Build system with FetchContent for GLAD 2 and GLM | VERIFIED | 79 lines; `glad_add_library(glad_gl STATIC REPRODUCIBLE LOADER API gl:core=4.1)` at line 19; `find_package(glfw3 3.4 REQUIRED)` at line 45; `CMAKE_CXX_STANDARD 20` at line 4 |
| `src/main.cpp` | Game loop: clear, draw scene to FBO, dither pass, swap | VERIFIED | 190 lines (exceeds 50-line minimum); full FBO + dither + ImGui pipeline at lines 135-183 |
| `src/engine/Window.h` | GLFW window wrapper with OpenGL 4.1 Core Profile context | VERIFIED | Contains `class Window` with all required methods; `Window.cpp` sets `GLFW_OPENGL_FORWARD_COMPAT` and validates GL version |
| `src/renderer/Framebuffer.h` | FBO wrapper with GL_RGB16F color + GL_DEPTH_COMPONENT24 | VERIFIED | `class Framebuffer` present; `.cpp` confirms `GL_RGB16F` + `GL_DEPTH_COMPONENT24` + `GL_NEAREST` filters; throws on incomplete FBO |
| `src/renderer/DitherPass.h` | Fullscreen quad Bayer dither post-process | VERIFIED | `class DitherPass` with `apply()` signature; `.cpp` loads `dither.frag`, creates VAO/VBO, draws 6 vertices |
| `assets/shaders/dither.frag` | 8x8 Bayer matrix with sphere-map world-space anchoring | VERIFIED | Contains `bayer8[64]`, `sphereUV()`, `uInverseView`, `step(threshold, luma)`, `#version 410 core` — all 5 required patterns present |
| `assets/shaders/scene.vert` | Vertex shader with MVP transform | VERIFIED | `#version 410 core`, `uModel/uView/uProjection` uniforms, `vWorldPos`+`vNormal` outputs |

#### From 01-02-PLAN.md must_haves

| Artifact | Provides | Status | Evidence |
|----------|----------|--------|----------|
| `src/renderer/Renderer.h` | Forward Blinn-Phong renderer with point light array | VERIFIED | `struct PointLight` with `position/color/radius/intensity`; `struct RenderObject`; `class Renderer` with `drawScene()` |
| `src/game/CathedralScene.h` | Cathedral test scene with pillars, arches, floor, and point lights | VERIFIED | `class CathedralScene`; `.cpp` builds 11 RenderObjects (floor, ceiling, wall, 6 pillars, 2 arches) + 5 PointLights with non-zero values |
| `src/engine/ImGuiLayer.h` | Dear ImGui integration for GLFW + OpenGL 4.1 | VERIFIED | `class ImGuiLayer` + `struct DebugParams` with all required fields (thresholdBias, patternScale, internalResIndex, resolutionChanged, cameraPos, cameraDir, cameraFov, cameraSpeed, fps, frameTimeMs, drawCalls) |
| `assets/shaders/scene.frag` | Blinn-Phong with smooth quartic attenuation for 1-bit output | VERIFIED | `struct PointLight`, `uPointLights[8]`, `pow(dist / radius, 4.0)` quartic attenuation, `uCameraPos` — all present |

---

### Key Link Verification

#### From 01-01-PLAN.md key_links

| From | To | Via | Status | Evidence |
|------|----|-----|--------|----------|
| `src/main.cpp` | `src/renderer/Framebuffer` | `sceneFBO.bind()` before draw, `sceneFBO.unbind()` after | WIRED | `main.cpp:135` `sceneFBO.bind()`; `main.cpp:141` `renderer.drawScene(...)` between; `main.cpp:143` `sceneFBO.unbind()` — draw is correctly sandwiched |
| `src/renderer/DitherPass` | `assets/shaders/dither.frag` | Shader loads dither.frag | WIRED | `DitherPass.cpp:7-10` `std::make_unique<Shader>("assets/shaders/dither.vert", "assets/shaders/dither.frag")` |
| `assets/shaders/dither.frag` | `src/main.cpp` | `uInverseView` uniform set per frame from camera | WIRED | `dither.frag:4` declares `uniform mat4 uInverseView`; `main.cpp:154` computes `glm::inverse(viewMatrix)`; `main.cpp:155-157` passes it to `ditherPass.apply(..., inverseView, ...)`; `DitherPass.cpp:63` calls `shader_->setMat4("uInverseView", inverseView)` |

#### From 01-02-PLAN.md key_links

| From | To | Via | Status | Evidence |
|------|----|-----|--------|----------|
| `src/game/CathedralScene` | `src/renderer/Renderer` | Scene provides mesh list and light array | WIRED | `main.cpp:141` `renderer.drawScene(scene.objects(), scene.lights(), ...)` — both arrays passed |
| `src/renderer/Renderer` | `assets/shaders/scene.frag` | Renderer sets point light uniforms per frame | WIRED | `Renderer.cpp:26-32` sets `uPointLights[i].position/color/radius/intensity`; `scene.frag:15` declares `uniform PointLight uPointLights[8]` |
| `src/engine/ImGuiLayer` | `src/main.cpp` | ImGuiLayer sliders modify dither/light/camera params | WIRED | `ImGuiLayer.cpp:49-57` has `SliderFloat("Threshold Bias"`, `SliderFloat("Pattern Scale"`, `Combo("Resolution"`); `main.cpp:155-157` passes `debugParams.thresholdBias` and `debugParams.patternScale` to `ditherPass.apply()` every frame |
| `assets/shaders/scene.frag` | `src/renderer/DitherPass` | Scene frag outputs luminance that dither pass thresholds | WIRED | `scene.frag:50` outputs `fragColor = vec4(vec3(clamp(totalLight,0,1)),1.0)`; FBO color texture (`sceneFBO.colorTexture()`) is passed to `ditherPass.apply()` at `main.cpp:155`; `dither.frag:46` samples it via `texture(sceneColor, vTexCoord).rgb` |

---

### Data-Flow Trace (Level 4)

This is a C++ native application. Data flows are render pipeline stages (uniform data to shaders), not HTTP/DB calls.

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|--------------------|--------|
| `dither.frag` | `uInverseView` | `glm::inverse(viewMatrix)` computed per frame in `main.cpp:154` from live WASD/arrow-key camera | Yes — live 4x4 matrix changes every frame camera moves | FLOWING |
| `dither.frag` | `uThresholdBias` | `debugParams.thresholdBias` (ImGui slider, default 0.0f) passed in `ditherPass.apply()` every frame | Yes — user-tunable float written unconditionally each frame | FLOWING |
| `dither.frag` | `uPatternScale` | `debugParams.patternScale` (ImGui slider, default 256.0f) passed in `ditherPass.apply()` every frame | Yes — user-tunable float written unconditionally each frame | FLOWING |
| `dither.frag` | `sceneColor` texture | `sceneFBO.colorTexture()` — FBO rendered with actual 3D Blinn-Phong scene each frame | Yes — actual rendered scene pixels from `renderer.drawScene()` with 5 live point lights | FLOWING |
| `scene.frag` | `uPointLights[i]` | `scene.lights()` from `CathedralScene` — 5 PointLight instances with non-zero position/radius/intensity | Yes — all 5 lights have intensity > 0 and radius > 0 (verified in `CathedralScene.cpp:70-103`) | FLOWING |
| `Framebuffer` | GL_RGB16F color target | `renderer.drawScene()` — full Blinn-Phong scene write per frame | Yes — depth-tested scene geometry written with real lighting; `GL_FRAMEBUFFER_SRGB` absent (no tone-mapping corruption) | FLOWING |

---

### Behavioral Spot-Checks

| Behavior | Check | Result | Status |
|----------|-------|--------|--------|
| Binary exists and is executable | `ls -la build/roguelike` | `-rwxr-xr-x 2323080 bytes, dated Mar 23 02:34` | PASS |
| Clean build with no errors | `cmake --build . 2>&1` | `[100%] Built target roguelike` — zero errors | PASS |
| `GL_FRAMEBUFFER_SRGB` absent (critical anti-pattern for dither correctness) | grep across all source | No matches | PASS |
| `GL_NEAREST` filter on FBO color texture | `Framebuffer.cpp:19-20` | `GL_NEAREST` for both `MIN_FILTER` and `MAG_FILTER` — confirmed | PASS |
| 5 PointLights pushed with non-zero values | `CathedralScene.cpp:70-103` | 5 PointLight instances; all have `intensity > 0` and `radius > 0` | PASS |
| `ditherPass.apply()` receives live `inverseView` (not identity) | `main.cpp:154-157` | `glm::inverse(viewMatrix)` from live `lookAt()` matrix — not a constant | PASS |
| ImGui `beginFrame`/`endFrame` always paired (Pitfall 5) | `main.cpp:162,173` | Both in same loop body with no early-exit or `return` between them | PASS |
| No TODO/FIXME/PLACEHOLDER anti-patterns in any source file | grep across all .cpp/.frag | 0 matches across all 8 key files | PASS |
| `gladLoadGL(glfwGetProcAddress)` used (GLAD 2 API, not GLAD 1) | `Window.cpp:26` | `gladLoadGL(glfwGetProcAddress)` confirmed — not `gladLoadGLLoader` | PASS |
| `GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE` set (required on macOS) | `Window.cpp:15` | Present — without this, context creation fails on macOS | PASS |

Step 7b: SKIPPED for automated behavioral execution — the application requires a display context (OpenGL window) and cannot be exercised headlessly. All 10 static spot-checks above pass.

---

### Requirements Coverage

All 5 Phase 1 requirement IDs are claimed across the two plan files: RNDR-01 through RNDR-04 in `01-01-PLAN.md`, RNDR-05 in `01-02-PLAN.md`. REQUIREMENTS.md traceability table maps all 5 to Phase 1 and marks them Complete.

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| RNDR-01 | 01-01-PLAN.md | Engine renders 3D scene to framebuffer with depth buffer | SATISFIED | `Framebuffer.cpp` creates FBO with `GL_RGB16F` color + `GL_DEPTH_COMPONENT24` depth; `main.cpp:135-143` renders to FBO with `glEnable(GL_DEPTH_TEST)` |
| RNDR-02 | 01-01-PLAN.md | Post-process 1-bit dithering shader converts scene to pure black and white using Bayer matrix | SATISFIED | `dither.frag` implements full 8x8 Bayer matrix at lines 11-24; `step(threshold, luma)` at line 50 guarantees strict 0/1 output; `fragColor = vec4(vec3(bit), 1.0)` at line 51 |
| RNDR-03 | 01-01-PLAN.md | Dither pattern uses world-space anchoring to prevent swimming during camera movement | SATISFIED | `dither.frag:27-31` `sphereUV(worldDir)` maps world direction to Bayer index; `uInverseView` set per frame from `glm::inverse(viewMatrix)` at `main.cpp:154`; `DitherPass.cpp:63` delivers it to the shader |
| RNDR-04 | 01-01-PLAN.md | Scene renders at low internal resolution and nearest-neighbor upscales to display | SATISFIED | FBO created at 854x480 (`RES_W[0]=854, RES_H[0]=480`); `GL_NEAREST` filter on color texture; dither pass renders fullscreen quad at `window.width() x window.height()` (display resolution) via `glViewport` in `DitherPass.cpp:68` |
| RNDR-05 | 01-02-PLAN.md | Point light sources (torches) affect dither density in their radius | SATISFIED | Blinn-Phong + quartic attenuation in `scene.frag:20-22`; 5 PointLights in `CathedralScene` with varying radii (6-10 units) and intensities (0.6-1.0); luminance output drives dither threshold end-to-end |

**Orphaned requirements check:** REQUIREMENTS.md maps only RNDR-01 through RNDR-05 to Phase 1. Both plans collectively claim all 5. No orphaned requirements.

---

### Anti-Patterns Found

| File | Pattern | Severity | Assessment |
|------|---------|----------|------------|
| None | — | — | No TODOs, FIXMEs, empty implementations, hardcoded empty returns, or placeholder patterns found in any source file |

Specific checks passed:
- No `GL_FRAMEBUFFER_SRGB` anywhere in source (would corrupt dither threshold comparisons)
- No `return null` / `return {}` / `return []` stub patterns in renderer files
- `Framebuffer.cpp` explicitly checks `GL_FRAMEBUFFER_COMPLETE` and throws on failure — not silently swallowed
- `Window.cpp` validates GL version >= 4.1 and throws on failure — not silently swallowed
- ImGui `beginFrame`/`endFrame` always paired within one loop iteration — no early-exit risk

---

### Human Verification Required

All automated code checks pass. The following 5 items require running the executable and observing output. These verify perceptual quality of correct code, not correctness of the code itself.

#### 1. Pure 1-bit Black-and-White Output

**Test:** Run `cd /Users/ozan/Projects/gsd-3d-roguelike/build && ./roguelike` and observe the window
**Expected:** Every pixel is either pure black (0,0,0) or pure white (1,1,1) — no gray values anywhere, no color
**Why human:** `step(threshold, luma)` guarantees binary output when both are finite floats, but confirming the absence of any grayscale pixel in a rendered frame requires visual inspection

#### 2. World-Space Dither Anchoring (No Swimming)

**Test:** With the application running, press arrow keys to rotate the camera view while watching the dither pattern on a static surface (floor or wall)
**Expected:** The dither pattern stays locked to the surface — it does not crawl, swim, or shift as the camera rotates. Mild sub-pixel drift on translation is acceptable (Obra Dinn tradeoff documented in RESEARCH.md)
**Why human:** Temporal frame-to-frame dither stability cannot be assessed from static code; requires observing two consecutive frames during camera motion

#### 3. Chunky Dither Cells at 480p Upscale

**Test:** At 1280x720 display resolution, observe the size of individual dither cells on the scene geometry
**Expected:** Each Bayer cell should be multiple pixels wide — visibly blocky/pointillist, not fine sub-pixel noise
**Why human:** Apparent cell size depends on interaction of 854x480 internal resolution, 1280x720 display resolution, GL_NEAREST upscale, and `uPatternScale` (default 256.0); computing perceptual size requires visual calibration

#### 4. Point Light Dither Density Gradients

**Test:** Look at the regions near each of the 5 point lights vs. far corners of the cathedral scene
**Expected:** Areas within a light's radius show noticeably more white pixels (higher dither density); dark corners show mostly black; no hard concentric ring artifacts visible around any light
**Why human:** Gradient smoothness and ring-artifact absence are perceptual qualities of the quartic attenuation formula that require seeing the actual rendered output

#### 5. ImGui Runtime Tuning

**Test:** Open the ImGui debug overlay; drag the "Threshold Bias" slider; then select a different resolution from the "Resolution" combo
**Expected:** Black/white balance shifts visibly as Threshold Bias moves; selecting a different resolution visibly changes dither cell size without recompiling the app; FPS counter updates each frame
**Why human:** UI responsiveness and the FBO resize side-effect require interactive runtime testing

---

### Summary

All Phase 1 code is present, substantive, wired, and data-flowing. The build compiles and links cleanly to a 2.3 MB executable. All 5 success criteria from ROADMAP.md have correct end-to-end implementations:

1. **SC-1 (pure B&W):** `step(threshold, luma)` in `dither.frag:50` — binary output guaranteed by shader logic; `GL_FRAMEBUFFER_SRGB` absent
2. **SC-2 (no swimming):** Sphere-map world-space anchoring via `uInverseView` + `sphereUV()` — mechanism fully implemented and wired per frame
3. **SC-3 (chunky cells):** 854x480 FBO + `GL_NEAREST` + display-resolution blit — pipeline correctly staged
4. **SC-4 (light density):** 5 PointLights + Blinn-Phong + quartic attenuation feeding dither threshold — end-to-end wired
5. **SC-5 (ImGui tuning):** All 4 collapsible sections (Dither, Camera, Performance, Lights) implemented with correct slider bindings — runtime parameter changes reach the pipeline every frame

All 5 requirement IDs (RNDR-01 through RNDR-05) are satisfied by concrete implementations. No orphaned requirements. No anti-patterns. The 5 human verification items above are recommended as final visual sign-off; they test the perceptual quality of correct code, not the correctness of the code itself.

---

_Verified: 2026-03-23T12:00:00Z_
_Verifier: Claude (gsd-verifier)_
