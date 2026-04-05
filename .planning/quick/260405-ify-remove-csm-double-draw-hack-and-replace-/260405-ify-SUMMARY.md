---
phase: quick
plan: 260405-ify
subsystem: rendering
tags: [opengl, shadow-mapping, csm, polygon-offset, performance]

# Dependency graph
requires: []
provides:
  - CSM shadow pass draws each caster once instead of twice (glPolygonOffset replaces double-draw)
affects: [rendering, shadow-mapping]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Use glPolygonOffset(1.1, 4.0) for shadow bias instead of CPU-side normal offset double-draw"]

key-files:
  created: []
  modified:
    - assets/shaders/engine/csm_depth.vert
    - src/engine/rendering/SceneRenderPipeline.cpp

key-decisions:
  - "glPolygonOffset(1.1, 4.0): factor=1.1 scales depth slope, units=4.0 adds constant — standard shadow mapping values"
  - "Keep uShadowCasterOffset (0.18) light-direction push — prevents peter-panning, unrelated to acne bias"
  - "Remove aNormal vertex attribute from csm_depth.vert since normal offset is no longer needed"

patterns-established:
  - "Shadow acne prevention via GL_POLYGON_OFFSET_FILL, not CPU double-draw"

requirements-completed: []

# Metrics
duration: 5min
completed: 2026-04-05
---

# Quick Task 260405-ify: Remove CSM Double-Draw Hack Summary

**Halved CSM shadow draw calls by replacing CPU-side normal-offset double-draw with GPU-level glPolygonOffset(1.1, 4.0)**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-04-05T00:00:00Z
- **Completed:** 2026-04-05T00:05:00Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments

- Removed double-draw loop (two `object.mesh->draw()` calls per shadow caster) from CSM render pass
- Replaced with `glEnable(GL_POLYGON_OFFSET_FILL)` + `glPolygonOffset(1.1f, 4.0f)` around a single draw loop
- Stripped `aNormal` vertex attribute and `uShadowNormalOffset` uniform from `csm_depth.vert` since normal-offset bias is no longer needed
- Preserved `uShadowCasterOffset` (light-direction push) which prevents peter-panning and is separate from acne bias

## Task Commits

1. **Task 1: Replace CSM double-draw with glPolygonOffset** - `01e191a`

## Files Created/Modified

- `assets/shaders/engine/csm_depth.vert` - Removed aNormal input, uShadowNormalOffset uniform, and normal offset math
- `src/engine/rendering/SceneRenderPipeline.cpp` - Removed double-draw loop, added glPolygonOffset bracket around single-draw loop

## Decisions Made

- Used `glPolygonOffset(1.1f, 4.0f)` — standard shadow mapping bias values (factor scales depth slope, units adds constant offset)
- Kept `uShadowCasterOffset = 0.18f` (light-direction push) intact — it prevents peter-panning and is orthogonal to acne bias

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## Self-Check: PASSED

- `assets/shaders/engine/csm_depth.vert` — FOUND
- `src/engine/rendering/SceneRenderPipeline.cpp` — FOUND
- Commit `01e191a` — FOUND (verified via git log)
- Build succeeded for both `level-editor` and `pixel-roguelike` targets

---
*Phase: quick*
*Completed: 2026-04-05*
