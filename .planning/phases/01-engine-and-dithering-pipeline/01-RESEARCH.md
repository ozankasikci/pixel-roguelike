# Phase 1: Engine and Dithering Pipeline - Research

**Researched:** 2026-03-23
**Domain:** Custom C++ OpenGL 4.1 engine with 1-bit Bayer dithering post-process pipeline
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Use 8x8 Bayer matrix for the dithering pattern (64 threshold levels, smooth gradients)
- **D-02:** Internal render resolution is configurable via ImGui slider, starting at 480p
- **D-03:** Dither threshold comparison color space is Claude's discretion (research suggests linear space for smoother light transitions)
- **D-04:** Overall brightness/contrast should be balanced — equal black/white distribution, not dark-heavy
- **D-05:** Dependency management approach is Claude's discretion (research suggested CMake FetchContent or vcpkg)
- **D-06:** Source tree organized by system: src/engine/, src/renderer/, src/game/ etc.
- **D-07:** ImGui debug overlay exposes: dither parameters (matrix size, threshold bias, internal resolution), light controls (add/move/adjust point lights), camera info (position, direction, FOV, speed), and performance metrics (FPS, draw calls, frame time)
- **D-08:** Test scene uses a cathedral mockup (simple arches, pillars, floor) to validate the aesthetic early
- **D-09:** Test lighting uses multiple point lights (3-5) at varying distances to validate falloff through the dithering pass

### Claude's Discretion

- Dither threshold color space (linear vs gamma — lean toward linear per research)
- Dependency management approach (FetchContent vs vcpkg vs vendored)
- Specific OpenGL extension usage within 4.1 Core Profile constraints
- Build system details (CMake target structure, compile flags)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| RNDR-01 | Engine renders 3D scene to framebuffer with depth buffer | Offscreen FBO with GL_DEPTH_ATTACHMENT; all features present in OpenGL 4.1 |
| RNDR-02 | Post-process 1-bit dithering shader converts scene to pure black and white using Bayer matrix | 8x8 Bayer matrix fullscreen quad fragment shader reading color attachment; well-documented Obra Dinn pattern |
| RNDR-03 | Dither pattern uses world-space anchoring to prevent swimming during camera movement | Sphere-map approach: reconstruct view direction per fragment, map to sphere UV, use as Bayer index; avoids depth reconstruction complexity |
| RNDR-04 | Scene renders at low internal resolution and nearest-neighbor upscales to display | FBO at 480-720p, blit to window framebuffer with GL_NEAREST; nearest-neighbor is explicit in OpenGL blit |
| RNDR-05 | Point light sources (torches) affect dither density in their radius | Forward Blinn-Phong lighting in scene shader; luminance output feeds dither threshold comparison |
</phase_requirements>

---

## Summary

This phase establishes the complete rendering foundation for the 1-bit dithered aesthetic — the game's core visual identity. The pipeline follows the Return of the Obra Dinn model: render a full 3D scene (with forward Blinn-Phong lighting) to an offscreen floating-point FBO at a reduced internal resolution, then apply a fullscreen Bayer matrix dither pass to produce pure black and white output, then nearest-neighbor blit to the display. Three architectural decisions must be locked before any code is written: color space (linear throughout, gamma correction only at output), world-space dither anchoring (sphere-map approach), and internal resolution pipeline (FBO at 480p, GL_NEAREST upscale). Missing any of these in Phase 1 requires expensive retrofitting later.

The platform is macOS development with OpenGL 4.1 Core Profile as the hard ceiling. This is a confirmed, non-negotiable constraint: Apple stopped OpenGL updates at 4.1 in 2018 and it applies to Apple Silicon (M4 Max) as well as Intel Macs. All features required for this phase — FBOs, GLSL 4.10, fullscreen quads, `GL_ARB_get_program_binary` — are available in 4.1. The key macOS-specific requirement is that the GLFW context must be created with `GLFW_OPENGL_FORWARD_COMPAT = GL_TRUE` alongside `GLFW_OPENGL_CORE_PROFILE`; macOS refuses to create a core profile context without the forward-compat flag. The Homebrew GLFW 3.4 on this machine is confirmed arm64-native and functional.

The dependency management recommendation is CMake FetchContent. GLAD 2 must be configured against `gl:core=4.1` (not 4.6), using the `SOURCE_SUBDIR cmake` flag in FetchContent. Dear ImGui is not available via Homebrew and must be fetched via FetchContent and manually wired as a CMake library target (ImGui has no native CMake support). All other Phase 1 libraries (GLM 1.0.3, spdlog 1.17.0) are available via Homebrew or FetchContent with known tags.

**Primary recommendation:** Build the complete dither pipeline (FBO → Bayer shader → nearest-neighbor blit) with world-space anchoring and correct linear color space in Wave 1. Do not proceed to lighting or ImGui until the dither output on static geometry is visually verified correct. The dither is the identity of the entire game — it must be locked before anything else depends on it.

---

## Project Constraints (from CLAUDE.md)

Directives extracted from `./CLAUDE.md` that the planner must verify compliance with:

| Directive | Impact on This Phase |
|-----------|----------------------|
| No Unity/Unreal/Godot | Confirmed: custom C++ + OpenGL only |
| Graphics API: OpenGL (resolved by research — 4.1 Core Profile, macOS cap) | All shader and FBO code targets GLSL 4.10 and OpenGL 4.1 |
| Visual style: strictly 1-bit black and white, no grayscale, no color | The dither pass output MUST be binary `vec4(0.0)` or `vec4(1.0)` — no intermediate values permitted |
| Platform: Desktop (macOS primary for development) | Must use `GLFW_OPENGL_FORWARD_COMPAT = GL_TRUE` and `-framework Cocoa/OpenGL/IOKit` link flags |
| Custom C++ engine | No game engine SDKs; OpenGL, GLFW, GLM, EnTT, ImGui only |
| GSD workflow: use `/gsd:execute-phase` for planned work | Planning artifact |

---

## Standard Stack

### Core (Phase 1 required)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 | standard | Implementation language | Required by EnTT v3.x; `std::span`, designated initializers reduce boilerplate |
| CMake | 4.1.1 (installed) | Build system | Confirmed present on dev machine; FetchContent handles all deps |
| GLFW | 3.4 (installed, arm64) | Window, OpenGL 4.1 context, input | Confirmed Homebrew arm64 build present; focused windowing API |
| GLAD 2 | v2.0.8 (FetchContent) | OpenGL 4.1 function loader | Replaces stagnant GLEW; `glad_add_library(glad LOADER API gl:core=4.1)` |
| GLM | 1.0.3 (Homebrew) | Math: vec3, mat4, quaternions | Confirmed installed; GLSL-identical syntax |
| Dear ImGui | v1.92.6 (FetchContent) | Runtime debug overlay | Not in Homebrew; must use FetchContent with manual CMake wiring |
| spdlog | 1.17.0 (Homebrew) | Structured logging | Confirmed installed; header-only |

### OpenGL 4.1 Feature Availability (macOS)

| Feature | Available in 4.1 | Notes |
|---------|-----------------|-------|
| Framebuffer Objects (FBO) | YES | `glGenFramebuffers`, `glBindFramebuffer` |
| GLSL 4.10 | YES | `#version 410 core` |
| `GL_ARB_get_program_binary` | YES | Promoted to core in 4.1; use for shader binary caching |
| `glBlitFramebuffer` with `GL_NEAREST` | YES | For nearest-neighbor upscale blit |
| Vertex Array Objects | YES | Required for Core Profile (no VAO = error) |
| Uniform Buffer Objects | YES | For per-frame camera/dither uniforms |
| Depth texture sampling | YES | `GL_DEPTH_COMPONENT` texture for depth read |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| CMake FetchContent | vcpkg | vcpkg is heavier; FetchContent keeps deps in CMakeLists.txt with no extra toolchain config |
| CMake FetchContent | Homebrew for all deps | ImGui not in Homebrew; mixing strategies creates confusion |
| GLAD 2 | libepoxy (in Homebrew) | libepoxy works but lacks GLAD 2's `glad_add_library` CMake integration |
| Forward rendering | Deferred rendering | Deferred needs G-buffer (multiple render targets); OpenGL 4.1 supports it but adds complexity without benefit for 3-5 lights |

### Installation

```bash
# Already installed on dev machine (Homebrew arm64):
# cmake 4.1.1, glfw 3.4, glm 1.0.3, spdlog 1.17.0

# Install openal-soft (for future phases, not Phase 1):
brew install openal-soft

# GLAD 2 via CMake FetchContent (in CMakeLists.txt):
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.8
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl STATIC REPRODUCIBLE LOADER API gl:core=4.1)

# Dear ImGui via FetchContent (requires manual CMakeLists.txt wrapper):
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.6
)
FetchContent_MakeAvailable(imgui)
# Then: add_library(imgui STATIC <imgui sources + backends>)
```

---

## Architecture Patterns

### Recommended Project Structure

```
src/
├── engine/              # Platform and core (D-06)
│   ├── Window.h/.cpp    # GLFW window + OpenGL 4.1 context
│   └── Input.h/.cpp     # GLFW callbacks -> InputEvent queue
├── renderer/            # All GPU concerns (D-06)
│   ├── Framebuffer.h/.cpp   # FBO wrapper (color + depth attachments)
│   ├── ShaderManager.h/.cpp # Compile, link, cache GLSL programs
│   ├── MeshManager.h/.cpp   # VAO/VBO/EBO for geometry
│   ├── Renderer.h/.cpp      # Scene draw calls + forward lighting
│   └── DitherPass.h/.cpp    # Fullscreen quad Bayer post-process
├── game/                # Game-specific systems (D-06)
│   └── CathedralScene.h/.cpp  # Test scene: arches, pillars, 3-5 lights
├── assets/
│   └── shaders/
│       ├── scene.vert / scene.frag    # Geometry + Blinn-Phong lighting
│       └── dither.vert / dither.frag  # Fullscreen quad Bayer pass
└── main.cpp             # Engine init, game loop
```

### Pattern 1: Offscreen FBO + Dither Post-Process

**What:** Render the full 3D scene (geometry + lighting) to a floating-point offscreen FBO at the internal resolution. Read the color attachment in a second fullscreen-quad pass that applies the Bayer matrix threshold and writes pure black or white to the default framebuffer.

**When to use:** This is the only correct architecture for the 1-bit dithering effect. The dither sees the complete composited luminance image — not individual objects.

**Example:**
```cpp
// Source: Architecture pattern confirmed by Obra Dinn devlog + LearnOpenGL FBO guide
// Per-frame render:
sceneFBO.bind();                              // FBO at 480p internal resolution
glViewport(0, 0, internalW, internalH);
renderer.drawScene(camera, lights);           // all geometry + Blinn-Phong
sceneFBO.unbind();

ditherPass.apply(sceneFBO.colorTexture());    // reads FBO, writes 1-bit to default FB
// Default framebuffer blit handled inside DitherPass with GL_NEAREST
```

### Pattern 2: World-Space Dither Anchoring (Sphere-Map Approach)

**What:** Map the Bayer matrix to a virtual sphere centered on the camera. For each fragment in the dither pass, reconstruct the view direction from screen UV, convert to sphere UV coordinates, then use those as the Bayer index. The sphere is world-oriented (not screen-oriented), so camera rotation keeps the pattern stable. Camera translation causes mild drift — this is the same tradeoff Obra Dinn accepted.

**Why this approach over depth reconstruction:** Depth reconstruction (reconstructing world-space XY from depth) is more expensive per-fragment and requires a separate depth texture read. The sphere-map approach only needs screen UV and the inverse-view matrix (no depth buffer read needed in the dither pass).

**Example:**
```glsl
// Source: Derived from Lucas Pope devlog sphere-map technique
// dither.frag - GLSL 4.10

#version 410 core

uniform sampler2D sceneColor;
uniform mat4 uInverseView;       // inverse of view matrix (no projection)
uniform float uThresholdBias;    // ImGui-tunable offset

in vec2 vTexCoord;
out vec4 fragColor;

// 8x8 Bayer matrix (D-01: locked to 8x8)
const int bayer8[64] = int[64](
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
);

float bayerValue(ivec2 p) {
    int idx = (p.x % 8) + (p.y % 8) * 8;
    return float(bayer8[idx]) / 64.0;
}

// Sphere-map UV from view direction
vec2 sphereUV(vec3 dir) {
    // Latitude/longitude on unit sphere
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159265);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / 3.14159265;
    return vec2(u, v);
}

void main() {
    // Reconstruct view direction from screen UV
    vec2 ndc = vTexCoord * 2.0 - 1.0;
    vec4 viewDir4 = uInverseView * vec4(ndc, 0.0, 0.0);
    vec3 worldDir = normalize(viewDir4.xyz);

    // Map sphere UV to Bayer index (scale to matrix tile frequency)
    vec2 suv = sphereUV(worldDir);
    ivec2 bayerCoord = ivec2(suv * 256.0);  // scale controls pattern density

    float threshold = bayerValue(bayerCoord) + uThresholdBias;

    // Sample linear-space luminance (D-03: compare in linear space)
    vec3 linearColor = texture(sceneColor, vTexCoord).rgb;
    float luma = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));  // Rec.709

    // Pure 1-bit output (CLAUDE.md: no grayscale)
    float bit = step(threshold, luma);
    fragColor = vec4(vec3(bit), 1.0);
}
```

### Pattern 3: Internal Resolution + Nearest-Neighbor Upscale (RNDR-04)

**What:** Create the FBO at a configurable internal resolution (starting 480p per D-02). The dither pass outputs to the same FBO resolution. Then `glBlitFramebuffer` with `GL_NEAREST` upscales to the display resolution. The blit produces the chunky, visible dither cells.

**Example:**
```cpp
// Source: OpenGL 4.1 core API, glBlitFramebuffer
// After dither pass writes to ditherFBO at internalW x internalH:
glBindFramebuffer(GL_READ_FRAMEBUFFER, ditherFBO);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // default framebuffer
glBlitFramebuffer(
    0, 0, internalW, internalH,      // source rect
    0, 0, displayW, displayH,        // destination rect
    GL_COLOR_BUFFER_BIT,
    GL_NEAREST                        // nearest-neighbor (no interpolation)
);
```

**Internal resolutions (D-02 starting point):**
- 480p: 854x480 (16:9) — chunky dither cells, maximum retro character
- 540p: 960x540 — intermediate
- 720p: 1280x720 — finer cells, more geometric detail readable

### Pattern 4: Forward Blinn-Phong Lighting for 1-Bit Output (RNDR-05)

**What:** Simple forward rendering with Blinn-Phong point lights. The light contribution increases luminance, which increases dither density (more white pixels) in the lit region. Attenuation must be tuned for 1-bit output — physically-correct 1/r² produces harsh rings; use linear falloff or custom smooth attenuation.

**Key insight:** Tune attenuation with the dither pass active, not in pre-dither grayscale.

**Example (scene.frag key excerpt):**
```glsl
// Source: LearnOpenGL Multiple Lights + attenuation tuned for 1-bit
struct PointLight {
    vec3 position;
    vec3 color;
    float radius;       // influence radius
    float intensity;
};

// Soft attenuation for 1-bit: avoids hard edge rings
float attenuation(float dist, float radius) {
    // Smooth hermite falloff - empirically better than 1/r^2 for dithering
    float x = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    return x * x;
}
```

### Pattern 5: macOS-Specific GLFW Context Creation

**What:** macOS requires both `GLFW_OPENGL_CORE_PROFILE` and `GLFW_OPENGL_FORWARD_COMPAT` set to `GL_TRUE` for any OpenGL 3.2+ context. Missing `FORWARD_COMPAT` causes context creation failure on macOS.

**Example:**
```cpp
// Source: GLFW documentation + carette.xyz M1 Mac OpenGL guide (verified)
#define GL_SILENCE_DEPRECATION  // suppress Apple deprecation warnings

glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // REQUIRED on macOS

// macOS CMake link flags (required):
target_link_libraries(${PROJECT_NAME}
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework IOKit"
    glfw
)
```

### Pattern 6: Shader Startup Compilation

**What:** Compile all shaders during engine initialization in a loading step, not lazily on first draw. On OpenGL 4.1, use `GL_ARB_get_program_binary` (core in 4.1) to cache compiled binaries across launches.

**Why:** Prevents frame spikes when the dither shader is first used. With only 2-3 shaders in Phase 1 this is trivial to implement correctly from the start.

### Anti-Patterns to Avoid

- **Screen-space UV for Bayer index:** `ivec2(gl_FragCoord.xy) % 8` as the sole coordinate — produces swimming dither on camera rotation. Never do this for the world-rendered pass.
- **Dithering per-object in the scene shader:** Each mesh applies dithering independently. Wrong — transparent/overlapping objects composite incorrectly. Always post-process on the full composited image.
- **Full-resolution FBO:** FBO at 1920x1080 with an 8x8 Bayer matrix produces invisible sub-pixel noise. The FBO must be at 480-720p.
- **`GL_FRAMEBUFFER_SRGB` enabled globally:** Corrupts the linear-space luminance the dither pass reads. Enable only for the final blit, not the intermediate FBO.
- **God-object `Engine` class:** Every system coupled through one object. Use explicit dependency injection instead.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| OpenGL function loading | Custom `dlopen` wrapper | GLAD 2 (`gl:core=4.1`) | GLAD generates exactly the 4.1 Core API; handles platform differences |
| Math (vec3, mat4, quaternions) | Custom math types | GLM 1.0.3 | Header-only; GLSL-identical syntax; only maintained option with full spec coverage |
| ImGui backend wiring | Custom GLFW event forwarding to ImGui | ImGui's `imgui_impl_glfw.cpp` + `imgui_impl_opengl3.cpp` | Official backends handle all GLFW callback integration; ~200 lines of correct event mapping |
| Logging | `printf` or `std::cout` | spdlog 1.17 | Thread-safe; category-based sinks; fmt formatting |
| 8x8 Bayer matrix values | Compute at runtime | Static `int bayer8[64]` constant | The values are a fixed mathematical constant; embed directly in shader |

**Key insight:** The Bayer matrix is pure data — there is no implementation to write. Embed the 64-value array as a GLSL constant. The only code is the sampling and threshold comparison.

---

## Common Pitfalls

### Pitfall 1: Swimming Dither (Critical — must fix in Phase 1)

**What goes wrong:** Screen-space coordinates (`gl_FragCoord.x % 8`, `gl_FragCoord.y % 8`) as the Bayer index. The pattern is pinned to the screen, not the geometry. Camera rotation causes the pattern to crawl across every surface — nauseating in first-person over extended play.

**Why it happens:** It's the obvious one-liner. Screen-space dithering is correct for static images but wrong for a 3D game.

**How to avoid:** Use the sphere-map approach (Pattern 2). The view direction per fragment is derived from screen UV + inverse view matrix. This keeps the pattern stable during rotation. Camera translation still causes mild drift (Obra Dinn accepted this tradeoff).

**Warning signs:** Any dither shader using `ivec2(gl_FragCoord.xy) % 8` as sole pattern coordinate.

### Pitfall 2: Color Space Mismatch

**What goes wrong:** `GL_FRAMEBUFFER_SRGB` enabled on intermediate FBOs, or gamma correction applied before the dither threshold comparison. The luminance the dither compares against is already gamma-corrected — dark regions get too many black pixels, torch falloff looks harsh and ringed.

**How to avoid:** Render in linear space throughout. The FBO color attachment format is `GL_RGB16F` (floating-point, linear). Apply gamma correction only at the final blit to the window framebuffer. The dither threshold comparison operates on linear-space luminance.

**Warning signs:** `GL_FRAMEBUFFER_SRGB` enabled at any point other than the final window blit. FBO format set to `GL_SRGB8_ALPHA8`.

### Pitfall 3: Missing `GLFW_OPENGL_FORWARD_COMPAT` on macOS

**What goes wrong:** Context creation returns null or 2.1 fallback. GLFW silently creates an OpenGL 2.1 context instead of 4.1 Core Profile. All subsequent `glGenVertexArrays` calls fail silently or crash.

**How to avoid:** Set `GLFW_OPENGL_FORWARD_COMPAT = GL_TRUE` alongside `GLFW_OPENGL_CORE_PROFILE`. This is macOS-specific and required.

**Warning signs:** `glGetString(GL_VERSION)` reports "2.1" after GLFW init. Debug assertion `GLFW_CONTEXT_VERSION_MAJOR == 4` fails.

### Pitfall 4: Missing VAO for Every Draw Call

**What goes wrong:** OpenGL 4.1 Core Profile requires a bound VAO for all draw calls. The compatibility profile allowed drawing without a VAO; the core profile does not. Failing to bind a VAO before `glDrawArrays` or `glDrawElements` is an OpenGL error (silent on some drivers, crash on Apple's).

**How to avoid:** Create a VAO for every mesh. The fullscreen quad for the dither pass also needs its own VAO. Create VAO before VBO, bind both, set vertex attributes, then the VAO remembers the VBO binding and attribute layout.

**Warning signs:** RenderDoc reports `GL_INVALID_OPERATION` on draw calls. Works on Windows/Linux but crashes on macOS.

### Pitfall 5: ImGui `NewFrame()` / `Render()` Pairing

**What goes wrong:** ImGui state corruption if `ImGui::NewFrame()` is called without a matching `ImGui::Render()` in the same frame (e.g., early return on error). Produces assertion failures or garbage rendering.

**How to avoid:** Structure the game loop so ImGui begin/end are always paired. Guard ImGui calls behind `#ifdef DEBUG` compile flag so release builds exclude it entirely.

### Pitfall 6: Torch Attenuation Rings

**What goes wrong:** Physically-correct `1/r²` attenuation produces a visible hard ring in the dithered output. The sharp falloff creates a luminance step that the 8x8 Bayer matrix renders as a concentric dithered ring around each torch.

**How to avoid:** Use smooth hermite or quartic falloff (Pattern 4). Tune attenuation with the dither pass active. The dither pass must be working before any lighting tuning is meaningful.

---

## Code Examples

Verified patterns from official/authoritative sources:

### FBO Creation with Color and Depth (RNDR-01)

```cpp
// Source: LearnOpenGL Framebuffers (https://learnopengl.com/Advanced-OpenGL/Framebuffers)
// OpenGL 4.1 Core Profile
void Framebuffer::create(int width, int height) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Floating-point color attachment — linear space for dither pass (D-03)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0,
                 GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTexture, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthRBO);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
```

### Fullscreen Quad VAO (required for dither pass)

```cpp
// Source: LearnOpenGL pattern + Core Profile VAO requirement
float quadVerts[] = {
    // position     // texcoord
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

GLuint quadVAO, quadVBO;
glGenVertexArrays(1, &quadVAO);
glGenBuffers(1, &quadVBO);
glBindVertexArray(quadVAO);
glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
glEnableVertexAttribArray(0);  // position
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
glEnableVertexAttribArray(1);  // texcoord
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                      (void*)(2 * sizeof(float)));
glBindVertexArray(0);
```

### GLFW macOS Context Init

```cpp
// Source: GLFW documentation + verified on Apple Silicon
#define GL_SILENCE_DEPRECATION

glfwInit();
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required on macOS

GLFWwindow* window = glfwCreateWindow(1280, 720, "3D Roguelike", nullptr, nullptr);
glfwMakeContextCurrent(window);

// Verify we got 4.1
int major, minor;
glGetIntegerv(GL_MAJOR_VERSION, &major);
glGetIntegerv(GL_MINOR_VERSION, &minor);
assert(major == 4 && minor >= 1);  // fail fast on wrong version
```

### ImGui GLFW + OpenGL3 Integration

```cpp
// Source: imgui/backends/imgui_impl_glfw.h + imgui_impl_opengl3.h
// In CMakeLists.txt, wire imgui + backends manually (no native CMake in imgui repo)
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();

ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 410 core");  // match GLSL version

// Per-frame in the render loop:
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

// ... ImGui widgets (D-07: dither params, light controls, camera info, perf) ...

ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| GLEW for OpenGL loading | GLAD 2 | GLAD 2 generates only requested API surface; GLEW stagnant since 2017 |
| Screen-space Bayer dithering | Sphere-mapped or world-space anchored | Screen-space acceptable for static images only; world-space required for 3D camera movement |
| `1/r²` attenuation for torches | Custom smooth falloff | Physical accuracy is wrong for 1-bit aesthetic; hermite/quartic falloff produces smooth dithered gradients |
| GLSL 4.60 examples in docs | GLSL 4.10 on macOS | All GLSL 4.10 features sufficient for this pipeline; update `#version` directive |

**Note on OpenGL 4.6 vs 4.1:** Prior project research (STACK.md) recommended 4.6. The locked constraint is 4.1 (macOS ceiling). All Phase 1 features (FBOs, GLSL 4.10, VAOs, UBOs, `glBlitFramebuffer`, `GL_ARB_get_program_binary`) are available in 4.1. The `#version` directive in all shaders must be `410 core`, not `460 core`.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Clang/C++20 | Build | YES | Apple Clang 17.0.0 | — |
| CMake | Build | YES | 4.1.1 | — |
| GLFW 3.4 | Window/context | YES (arm64) | 3.4 | — |
| GLM | Math | YES (Homebrew) | 1.0.3 | — |
| spdlog | Logging | YES (Homebrew) | 1.17.0 | — |
| GLAD 2 | OpenGL loader | NOT INSTALLED | via FetchContent | — |
| Dear ImGui | Debug overlay | NOT INSTALLED | via FetchContent v1.92.6 | — |
| OpenGL.framework | GPU API | YES | 4.1 (macOS hard cap) | — |
| openal-soft | Audio (future phases) | NOT INSTALLED | brew install | Defer to Phase 6 |

**Missing dependencies with no fallback (must install during Wave 0):**
- GLAD 2: must be fetched via CMake FetchContent; required before any OpenGL calls
- Dear ImGui: must be fetched via CMake FetchContent with manual CMake wiring; required for D-07 debug overlay

**Platform notes:**
- macOS 26.1, Apple M4 Max confirmed. GLFW arm64 native build present.
- OpenGL.framework present at `/System/Library/Frameworks/OpenGL.framework/`
- `#define GL_SILENCE_DEPRECATION` required before OpenGL headers to suppress Apple deprecation warnings

---

## Open Questions

1. **Sphere-map UV scale for Bayer index**
   - What we know: The sphere UV is [0,1]x[0,1] per full sphere surface. Scaling to `ivec2(suv * 256)` before modulo-8 gives approximately 32 Bayer tiles across the sphere.
   - What's unclear: The ideal scale factor is aesthetic/empirical — too small produces oversized dither cells, too large produces fine noise. This needs tuning via the ImGui threshold bias slider (D-07).
   - Recommendation: Start at scale 256; expose as ImGui slider with range 64-512.

2. **Scene FBO color format: RGB16F vs RGB8**
   - What we know: `GL_RGB16F` preserves full linear-space HDR values for the luminance computation. `GL_RGB8` would quantize to 0-255 before the dither pass.
   - What's unclear: Whether `GL_RGB8` quantization introduces visible banding in the dither gradient for this project's lighting levels.
   - Recommendation: Use `GL_RGB16F`. The GPU cost is negligible at 480p internal resolution.

3. **ImGui docking branch vs main**
   - What we know: The docking branch enables multi-panel debug windows. The main branch is simpler.
   - What's unclear: Whether D-07's debug overlay benefits from docking (separate panels for dither/lights/camera/perf).
   - Recommendation: Use main branch (v1.92.6 tag) for Phase 1. Docking can be added later if needed.

---

## Sources

### Primary (HIGH confidence)

- Lucas Pope / Return of the Obra Dinn devlog — https://dukope.com/devlogs/obra-dinn/tig-32/ — sphere-map dithering, 8x8 Bayer + blue noise patterns, temporal stability tradeoffs
- LearnOpenGL Framebuffers — https://learnopengl.com/Advanced-OpenGL/Framebuffers — FBO creation, depth attachment, blit
- GLFW documentation — https://www.glfw.org/docs/latest/ — macOS context hints, `GLFW_OPENGL_FORWARD_COMPAT` requirement
- Apple OpenGL Capabilities Tables — https://developer.apple.com/opengl/OpenGL-Capabilities-Tables.pdf — 4.1 hard cap confirmed
- GLAD 2 GitHub discussions — https://github.com/Dav1dde/glad/discussions/426 — FetchContent `SOURCE_SUBDIR cmake` + `glad_add_library` usage
- Alex Charlton — Dithering on the GPU — https://alex-charlton.com/posts/Dithering_on_the_GPU/ — 8x8 Bayer matrix values, GLSL implementation
- Phoronix forums — macOS OpenGL 4.1 confirmed final — https://www.phoronix.com/forums/forum/linux-graphics-x-org-drivers/opengl-vulkan-mesa-gallium3d/1342697-confirming-apple-will-not-be-updating-opengl-on-macos-past-version-4-1
- carette.xyz M1 Mac OpenGL guide — https://carette.xyz/posts/opengl_and_cpp_on_m1_mac/ — GLFW hints, macOS framework link flags

### Secondary (MEDIUM confidence)

- Homebrew package info — glfw 3.4 arm64, glm 1.0.3, spdlog 1.17.0 confirmed installed (verified via `brew info`)
- File inspection — `libglfw.3.4.dylib: Mach-O 64-bit dynamically linked shared library arm64` (verified via `file` command)
- OpenGL.framework existence confirmed at `/System/Library/Frameworks/OpenGL.framework/`
- LearnOpenGL Multiple Lights — https://learnopengl.com/Lighting/Multiple-lights — Blinn-Phong point light attenuation structure
- Hacker News discussion on Obra Dinn dithering — https://news.ycombinator.com/item?id=47406160 — sphere-map vs depth-reconstruction tradeoffs

### Tertiary (LOW confidence)

- riptutorial macOS OpenGL 4.1 setup — https://riptutorial.com/opengl/example/21105/setup-modern-opengl-4-1-on-macos -- GLFW GLEW setup pattern (GLEW replaced by GLAD 2 in this project)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all library versions verified via `brew info`; arm64 GLFW confirmed installed
- Architecture patterns: HIGH — FBO+post-process confirmed by Obra Dinn devlog and LearnOpenGL; sphere-map approach confirmed by Pope's devlog
- World-space anchoring approach: MEDIUM — sphere-map confirmed conceptually; specific UV scale factor is empirical and needs tuning
- Pitfalls: HIGH — swimming dither from Obra Dinn devlog; macOS GLFW hint from official GLFW docs + multiple verified sources; color space from LearnOpenGL

**Research date:** 2026-03-23
**Valid until:** 2026-06-23 (stable domain; OpenGL 4.1 has been frozen since 2018)
