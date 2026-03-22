# Phase 1: Engine and Dithering Pipeline - Context

**Gathered:** 2026-03-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Custom C++ rendering engine with OpenGL (4.1 Core Profile for macOS compatibility), featuring a post-process 1-bit Bayer dithering pipeline. The pipeline renders 3D scenes at a configurable low internal resolution, applies an 8x8 Bayer matrix dithering pass with world-space anchoring, and nearest-neighbor upscales to display. Point light sources affect dither density. A Dear ImGui debug overlay enables real-time parameter tuning.

</domain>

<decisions>
## Implementation Decisions

### Dither Aesthetic
- **D-01:** Use 8x8 Bayer matrix for the dithering pattern (64 threshold levels, smooth gradients)
- **D-02:** Internal render resolution is configurable via ImGui slider, starting at 480p
- **D-03:** Dither threshold comparison color space is Claude's discretion (research suggests linear space for smoother light transitions)
- **D-04:** Overall brightness/contrast should be balanced — equal black/white distribution, not dark-heavy

### Build & Dependencies
- **D-05:** Dependency management approach is Claude's discretion (research suggested CMake FetchContent or vcpkg)
- **D-06:** Source tree organized by system: src/engine/, src/renderer/, src/game/ etc.

### Debug Tooling
- **D-07:** ImGui debug overlay exposes: dither parameters (matrix size, threshold bias, internal resolution), light controls (add/move/adjust point lights), camera info (position, direction, FOV, speed), and performance metrics (FPS, draw calls, frame time)

### Test Scene
- **D-08:** Test scene uses a cathedral mockup (simple arches, pillars, floor) to validate the aesthetic early
- **D-09:** Test lighting uses multiple point lights (3-5) at varying distances to validate falloff through the dithering pass

### Claude's Discretion
- Dither threshold color space (linear vs gamma — lean toward linear per research)
- Dependency management approach (FetchContent vs vcpkg vs vendored)
- Specific OpenGL extension usage within 4.1 Core Profile constraints
- Build system details (CMake target structure, compile flags)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project Context
- `.planning/PROJECT.md` — Project vision, core value (1-bit dithering is the identity), constraints (custom C++, macOS, no color)
- `.planning/REQUIREMENTS.md` — Phase 1 requirements: RNDR-01 through RNDR-05
- `.planning/ROADMAP.md` — Phase 1 goal and success criteria

### Research
- `.planning/research/STACK.md` — Technology stack recommendations (OpenGL 4.1, GLFW, GLAD v2, libraries)
- `.planning/research/ARCHITECTURE.md` — Engine architecture and build order
- `.planning/research/PITFALLS.md` — Critical pitfalls: swimming dither, color space, engine trap
- `.planning/research/SUMMARY.md` — Synthesized research findings

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- None — greenfield project, no existing code

### Established Patterns
- None — patterns will be established by this phase

### Integration Points
- This phase creates the foundation all subsequent phases build on
- The renderer and dithering pipeline must be stable before Phase 2 adds player movement and level geometry

</code_context>

<specifics>
## Specific Ideas

- Visual reference: Return of the Obra Dinn's 1-bit dithered aesthetic — full 3D geometry reduced to pure black and white
- Gothic cathedral setting with arches, pillars, and wall-mounted torches (from user's reference image)
- Must develop and run on macOS — OpenGL capped at 4.1 Core Profile
- World-space dither anchoring is critical — prevents pattern swimming during camera movement (Obra Dinn's hardest problem per developer logs)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-engine-and-dithering-pipeline*
*Context gathered: 2026-03-23*
