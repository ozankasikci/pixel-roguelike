# Phase 10: Global Keyboard Shortcuts and Hover Highlight - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

The editor responds to Delete, Ctrl+D, Escape, and F from any focused panel (viewport, outliner, inspector) with correct text-field safety guards. A hover highlight appears on objects under the cursor before clicking, visually distinct from the selection overlay.

</domain>

<decisions>
## Implementation Decisions

### Hover Highlight Style
- **D-01:** Subtle wireframe bounding box — same shape language as selection overlay but thinner (line width ~2.0) and a cool color (light blue/white) at ~60% opacity
- **D-02:** Depth-tested only — no ghost pass for occluded parts. If the hovered object is behind a wall, no hover wireframe shows. Keeps hover visually lightweight compared to selection.

### Hover Highlight Behavior
- **D-03:** No hover on already-selected objects — selected objects show gold selection wireframe only, no stacked hover
- **D-04:** Suppress hover during: right-mouse orbit/pan, gizmo drag, placement mode, and play preview capture
- **D-05:** Hover requires per-frame raycasting when cursor is over viewport (reuse `pickEditorObjects` infrastructure)

### Shortcut Text Safety
- **D-06:** Guard all single-key shortcuts (Delete, F, W, E, R, Escape) with `io.WantTextInput` — suppress when a text field is focused
- **D-07:** Modifier combos (Ctrl+D, Ctrl+Z, Ctrl+Y, Ctrl+S, Ctrl+B, Ctrl+R, Ctrl+P) remain active even in text fields since they don't conflict with text editing
- **D-08:** Escape in a text field deselects the text field first (ImGui default); next Escape press clears selection

### Frame Camera (F Key)
- **D-09:** Smooth animated framing — camera lerps to target position over ~0.3s with ease-out. Replaces current instant snap.
- **D-10:** F works with multi-selection — computes combined bounding box of all selected objects and frames to fit all. No longer limited to single selection.

### Claude's Discretion
- Specific lerp/easing function for camera animation (slerp for rotation, lerp for position, or combined orbit interpolation)
- Whether hover raycasting reuses `pickEditorObjects` directly or uses a lighter single-hit variant for performance
- `ImGuiInputFlags_RouteGlobal` usage vs current `ImGui::IsKeyPressed` approach — choose whatever correctly handles focus routing

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Selection System
- `src/editor/render/EditorScenePreviewRenderer.cpp` — `appendSelectionOverlays()` generates wireframe overlays; hover overlay will follow same pattern
- `src/editor/render/EditorScenePreviewRenderer.h` — Header for overlay rendering
- `src/editor/viewport/EditorViewportInteraction.cpp` — `pickEditorObjects()`, ray casting, hit detection
- `src/editor/viewport/EditorViewportInteraction.h` — `EditorHitResult`, `EditorSelectionPickerState`
- `src/editor/scene/EditorSelectionSystem.cpp` — Selection state management, `pruneSelection`

### Shortcuts and Input
- `apps/level_editor/main.cpp:420-460` — Current keyboard shortcut handling block
- `apps/level_editor/main.cpp:1708-1717` — Current F-key focus camera implementation

### Rendering
- `src/engine/rendering/geometry/Renderer.cpp:71-119` — `drawScene()` handles `ignoreDepth`, `wireframe`, `lineWidth` per RenderObject

### Prior Phase Context
- `.planning/phases/09-selection-overlay-depth-fix/09-CONTEXT.md` — Two-pass selection overlay decisions (Phase 9)

No external specs — requirements fully captured in decisions above.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `appendSelectionOverlays()` in EditorScenePreviewRenderer.cpp — can be extended or paralleled for hover overlay generation
- `pickEditorObjects()` + `EditorHitResult` in EditorViewportInteraction — reusable for hover hit detection
- `focusEditorCameraOnBounds()` and `focusEditorCameraOnPoint()` — existing snap-focus functions to convert to animated
- `viewportState.hovered` — already tracks whether cursor is over the viewport

### Established Patterns
- Selection overlays are `RenderObject` instances with `wireframe=true`, `unlit=true`, appended to render list
- Primary selection: gold (1.30, 0.92, 0.24), line width 4.0; secondary: cyan (0.50, 1.00, 0.62), line width 2.5
- Two-pass depth approach from Phase 9: ghost pass (ignoreDepth=true, 20% tint) + depth-tested primary pass
- Keyboard shortcuts live in `main.cpp` input handling block, set boolean flags consumed later in the frame

### Integration Points
- `main.cpp` input block (~line 426) — add WantTextInput guards to existing shortcut checks
- `EditorScenePreviewRenderer::appendSelectionOverlays()` — add hover overlay generation alongside selection overlays
- `main.cpp` viewport section — add per-frame hover raycast when cursor is over viewport
- `focusEditorCameraOnBounds` / `focusEditorCameraOnPoint` — replace with animated versions or add animation state to editor camera

</code_context>

<specifics>
## Specific Ideas

- Hover color should feel "cool" (blue/white family) to contrast with "warm" gold selection — reinforces the preview-vs-committed visual language
- Animation duration ~0.3s matches Unity/Unreal feel for camera framing
- Multi-selection frame should compute the union bounding box of all selected objects

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 10-global-keyboard-shortcuts-and-hover-highlight*
*Context gathered: 2026-04-01*
