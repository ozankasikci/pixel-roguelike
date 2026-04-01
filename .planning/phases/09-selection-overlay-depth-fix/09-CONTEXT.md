# Phase 9: Selection Overlay Depth Fix - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix the selection overlay so it renders depth-correctly in 3D space. Currently selection wireframe cubes use `ignoreDepth = true` causing them to bleed through walls and geometry in front of the selected object. The fix is a two-pass approach: depth-tested primary pass + faint ghost for fully occluded objects.

</domain>

<decisions>
## Implementation Decisions

### Outline Shape
- Keep wireframe bounding box (current approach) — no switch to mesh silhouette
- Two-pass rendering: depth-tested primary pass + faint ghost pass for fully occluded objects

### Ghost Outline Style
- Ghost outline uses same wireframe at 20% opacity
- No configurable toggle — ghost outlines are always on

### Claude's Discretion
- Specific OpenGL depth/blend state management for the two passes
- Whether to use stencil buffer or pure depth test approach
- Line width adjustments if needed for visual clarity

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `appendSelectionOverlays()` in `EditorScenePreviewRenderer.cpp:212-252` — creates wireframe cubes with `ignoreDepth = true`
- `Renderer::drawScene()` in `Renderer.cpp:71-119` — handles `ignoreDepth` flag per-object
- `RenderObject` struct has `ignoreDepth`, `wireframe`, `lineWidth`, `unlit` fields

### Established Patterns
- Selection overlays are appended to the render objects list and drawn in the same pass
- Primary selection: yellow/gold (1.30, 0.92, 0.24), line width 4.0
- Secondary selection: cyan (0.50, 1.00, 0.62), line width 2.5
- All selection overlays use "metal_default" material and `unlit = true`

### Integration Points
- `appendSelectionOverlays()` in EditorScenePreviewRenderer.cpp — main modification point
- `Renderer::drawScene()` in Renderer.cpp — needs support for two-pass depth behavior
- Called from `apps/level_editor/main.cpp:1321`

</code_context>

<specifics>
## Specific Ideas

No specific requirements — straightforward depth fix with two-pass approach for ghost outlines.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>
