# Phase 1: Engine and Dithering Pipeline - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-23
**Phase:** 01-engine-and-dithering-pipeline
**Areas discussed:** Dither aesthetic, Build & deps, Debug tooling, Test scene

---

## Dither Aesthetic

### Bayer Matrix Size

| Option | Description | Selected |
|--------|-------------|----------|
| 8x8 (Recommended) | Smoother gradients, closer to Obra Dinn look — 64 threshold levels | ✓ |
| 4x4 | Chunkier, more aggressive dithering — 16 threshold levels | |
| Both + toggle | Implement both, switch via ImGui to compare | |
| You decide | Claude picks what looks best | |

**User's choice:** 8x8 (Recommended)

### Internal Render Resolution

| Option | Description | Selected |
|--------|-------------|----------|
| 480p | Very chunky pixels, strong retro feel, closest to Obra Dinn | |
| 640p | Middle ground — visible dither cells without being too blocky | |
| 720p | Subtler dithering, more detail preserved | |
| Configurable | Start at 480p, let ImGui slider adjust at runtime | ✓ |

**User's choice:** Configurable

### Dither Threshold Color Space

| Option | Description | Selected |
|--------|-------------|----------|
| Linear (Recommended) | Smoother gradients at light boundaries — research flagged gamma as a known pitfall | |
| You decide | Claude picks based on what looks right | ✓ |

**User's choice:** You decide (Claude's discretion — lean linear per research)

### Brightness/Contrast

| Option | Description | Selected |
|--------|-------------|----------|
| Dark-heavy | Mostly black with bright highlights — matches reference image | |
| Balanced | Equal black/white distribution | ✓ |
| Configurable | Threshold bias slider in ImGui to tune live | |

**User's choice:** Balanced

---

## Build & Dependencies

### Dependency Management

| Option | Description | Selected |
|--------|-------------|----------|
| CMake FetchContent | Downloads deps at configure time — simple, no extra tools needed | |
| vcpkg | Package manager — more structured, better for many deps | |
| Vendored (git submodules) | Check deps into repo — full control, offline builds | |
| You decide | Claude picks based on project needs | ✓ |

**User's choice:** You decide (Claude's discretion)

### Source Tree Organization

| Option | Description | Selected |
|--------|-------------|----------|
| By system | src/engine/, src/renderer/, src/game/ — grouped by subsystem | ✓ |
| Flat | src/ with all files — simple for now, reorganize later | |
| You decide | Claude picks based on project scale | |

**User's choice:** By system

---

## Debug Tooling

### ImGui Debug Overlay Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Dither params | Matrix size, threshold bias, internal resolution slider | ✓ |
| Light controls | Add/move/adjust point lights in real-time | ✓ |
| Camera info | Position, direction, FOV, movement speed | ✓ |
| Performance | FPS counter, draw calls, frame time graph | ✓ |

**User's choice:** All four — full debug overlay

---

## Test Scene

### Test Geometry

| Option | Description | Selected |
|--------|-------------|----------|
| Cathedral mockup | Simple arches, pillars, floor — validates the final aesthetic early | ✓ |
| Primitives | Cubes, spheres, planes — easier to debug lighting/dithering math | |
| Both | Start with primitives, graduate to a cathedral mockup once dithering works | |

**User's choice:** Cathedral mockup

### Torch Lighting Test

| Option | Description | Selected |
|--------|-------------|----------|
| Multiple point lights | Place 3-5 lights at varying distances to test falloff through dithering | ✓ |
| Single light first | One moveable light via ImGui, add more later | |
| You decide | Claude picks the approach | |

**User's choice:** Multiple point lights

---

## Claude's Discretion

- Dither threshold color space (linear vs gamma)
- Dependency management approach
- OpenGL extension usage within 4.1 constraints
- Build system details

## Deferred Ideas

None — discussion stayed within phase scope
