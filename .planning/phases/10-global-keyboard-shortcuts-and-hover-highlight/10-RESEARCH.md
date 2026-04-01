# Phase 10: Global Keyboard Shortcuts and Hover Highlight - Research

**Researched:** 2026-04-01
**Domain:** ImGui input handling, editor viewport raycasting, RenderObject overlay rendering, camera animation
**Confidence:** HIGH

## Summary

Phase 10 adds four keyboard shortcuts and a viewport hover highlight to the level editor. All APIs required already exist in the codebase. The work is entirely in-engine with no third-party library additions.

The keyboard shortcuts (Delete, Ctrl+D, Escape, F) already have partial implementations in `apps/level_editor/main.cpp` (~line 426). The gaps are: (1) the `io.WantTextInput` guard is missing on Delete, F, and Escape; (2) the Escape handler does not clear `selectedIds`; (3) the F key only handles single selection and uses instant snap instead of the animated lerp required by D-09/D-10. The duplicate handler at line 1743 does not apply a visible position offset — `OBJ-02` requires one.

The hover highlight (SEL-02) follows the same `RenderObject` pattern as `appendSelectionOverlays`. The hover overlay is a single depth-tested wireframe bounding box (blue-white, line width 2.0, no ghost pass per D-02). It requires a per-frame raycast when the cursor is over the viewport, which can reuse `pickEditorObject` from `EditorSelectionSystem.h` (the first-hit-only variant of `pickEditorObjects`).

The camera smooth animation (D-09) needs a small `EditorCameraAnimation` state struct added alongside `EditorCamera`, because `EditorCamera` itself is a pure POD snapshot with no animation bookkeeping.

**Primary recommendation:** All changes are isolated to `main.cpp`, `EditorViewportController.cpp/.h`, and `EditorScenePreviewRenderer.cpp/.h`. No new files are needed if the animation state is added to `EditorViewportController.h`.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01:** Hover overlay: subtle wireframe bounding box — same shape language as selection overlay but thinner (line width ~2.0) and cool color (light blue/white) at ~60% opacity
**D-02:** Depth-tested only — no ghost pass for hover. If the hovered object is behind a wall, no hover wireframe shows.
**D-03:** No hover on already-selected objects — selected objects show gold selection wireframe only, no stacked hover
**D-04:** Suppress hover during: right-mouse orbit/pan, gizmo drag, placement mode, and play preview capture
**D-05:** Hover requires per-frame raycasting when cursor is over viewport — reuse `pickEditorObjects` infrastructure
**D-06:** Guard single-key shortcuts (Delete, F, W, E, R, Escape) with `io.WantTextInput` — suppress when a text field is focused
**D-07:** Modifier combos (Ctrl+D, Ctrl+Z, Ctrl+Y, Ctrl+S, Ctrl+B, Ctrl+R, Ctrl+P) remain active even in text fields
**D-08:** Escape in a text field deselects the text field first (ImGui default); next Escape press clears selection
**D-09:** Smooth animated framing — camera lerps to target position over ~0.3s with ease-out. Replaces current instant snap.
**D-10:** F works with multi-selection — computes combined bounding box of all selected objects and frames to fit all.

### Claude's Discretion

- Specific lerp/easing function for camera animation (slerp for rotation, lerp for position, or combined orbit interpolation)
- Whether hover raycasting reuses `pickEditorObjects` directly or uses `pickEditorObject` (first-hit single variant) for performance
- `ImGuiInputFlags_RouteGlobal` usage vs current `ImGui::IsKeyPressed` approach

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SEL-02 | Objects highlight on mouse hover before clicking | Hover ray via `pickEditorObject`, overlay via `appendSelectionOverlays` pattern; single depth-tested pass only |
| SEL-03 | Escape key clears all selection | Extend existing Escape handler in main.cpp; add `!io.WantTextInput` guard and `selectedIds.clear()` |
| OBJ-01 | Delete key removes selected object globally (viewport + outliner) | Existing `deletePressed` handler is complete except missing `!io.WantTextInput` guard |
| OBJ-02 | Ctrl+D duplicates selected object with position offset | Existing duplicate handler needs a post-duplicate `applyWorldTransform` call to translate by offset |
| OBJ-03 | F key frames camera on selected object | Extend existing `focusPressed` handler: add multi-selection union bounds, replace snap with lerp animation |
</phase_requirements>

---

## Standard Stack

### Core (already in project — no new dependencies)

| Library | Version | Purpose | Relevance to Phase |
|---------|---------|---------|-------------------|
| ImGui | v1.92.6 | Keyboard input, `io.WantTextInput` | `io.WantTextInput`, `ImGui::IsKeyPressed`, `ImGuiKey_*` |
| GLFW | 3.4 | Mouse button state for hover suppression | `glfwGetMouseButton` right-click check already used at line 444 |
| GLM | 1.0.3 | Camera lerp, bounding box union | `glm::mix`, `glm::length`, `glm::max`, `glm::min` |
| OpenGL 4.1 | — | Wire frame overlay rendering | `GL_LINE`, `glLineWidth`, `glPolygonMode` — already in Renderer.cpp |

No new packages. No new CMake targets. No Homebrew changes.

---

## Architecture Patterns

### Pattern 1: Keyboard shortcut guards with `io.WantTextInput`

**What:** ImGui exposes `io.WantTextInput` (bool) which is `true` whenever a text input widget has keyboard focus. Single-key shortcuts must be suppressed when this is true.

**Current state in main.cpp (~line 426):**
```cpp
// Already guarded — correct:
if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_P)) { ... }
if (!io.WantTextInput && glfwGetMouseButton(...) != GLFW_PRESS) {
    if (ImGui::IsKeyPressed(ImGuiKey_W)) ...
    if (ImGui::IsKeyPressed(ImGuiKey_E)) ...
    if (ImGui::IsKeyPressed(ImGuiKey_R)) ...
}

// MISSING guard — must be fixed:
if (ImGui::IsKeyPressed(ImGuiKey_F)) focusPressed = true;        // needs !io.WantTextInput
if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;  // needs !io.WantTextInput
if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { ... }                // needs !io.WantTextInput
```

**Fix pattern:**
```cpp
if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;
if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F)) focusPressed = true;
if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    placementState.clear();
    widgetCommand.clear();
    gizmoCommand.clear();
    selectedIds.clear();
    selectionPicker.clear();
    ui.inspectorContext = EditorInspectorContext::SceneSelection;
}
```

**Note on D-07:** Modifier combos (`Ctrl+D`, `Ctrl+Z`, etc.) do NOT need `!io.WantTextInput`. They are currently unguarded and that is correct per the decision. No change needed for those.

### Pattern 2: Hover Highlight Overlay

**What:** Before each render, if the cursor is over the viewport, not suppressed (D-04), and not in play preview mode, run `pickEditorObject` to get the first-hit object under the cursor. If it is not already selected, call `appendHoverOverlay` to add a single wireframe `RenderObject`.

**Hover overlay rendering — single depth-tested pass (D-02):**
```cpp
// In EditorScenePreviewRenderer.cpp — new function appendHoverOverlay:
void appendHoverOverlay(std::vector<RenderObject>& objects,
                        const EditorPreviewWorld& world,
                        const MaterialTextureLibrary& materials,
                        std::uint64_t hoveredId,
                        const std::vector<std::uint64_t>& selectedIds) {
    if (hoveredId == 0 || isSelected(selectedIds, hoveredId)) {
        return;  // D-03: no hover on selected objects
    }
    Mesh* cube = world.meshLibrary().get("cube");
    const EditorObjectBounds* bounds = world.findObjectBounds(hoveredId);
    if (cube == nullptr || bounds == nullptr || !bounds->valid) {
        return;
    }
    const glm::vec3 center = bounds->center();
    glm::vec3 size = glm::max(bounds->max - bounds->min, glm::vec3(0.12f));
    size *= 1.035f;

    // Single depth-tested pass only (D-02) — cool blue-white color
    objects.push_back(RenderObject{
        cube,
        makeModelMatrix(center, size),
        glm::vec3(0.55f, 0.85f, 1.00f),  // cool blue-white per D-01
        materials.resolve("metal_default"),
        true,   // wireframe
        false,  // ignoreDepth = false — depth-tested per D-02
        true,   // unlit
        2.0f    // lineWidth per D-01
    });
}
```

**Hover raycast in main.cpp (inside the scene-visible editor block, after `appendSelectionOverlays`):**
```cpp
// Compute hovered object ID per frame
std::uint64_t hoveredObjectId = 0;
const bool suppressHover = ui.playPreview
    || gameplayPreviewCaptured
    || placementState.active()
    || editorGizmoIsHot()
    || glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

if (!suppressHover && renderViewportState.hovered) {
    const EditorRay hoverRay = buildEditorRay(
        inverseViewProjection,
        glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
        glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
        glm::vec2(io.MousePos.x, io.MousePos.y));
    if (const auto hit = pickEditorObject(viewportSelectionHandles, hoverRay)) {
        hoveredObjectId = hit->objectId;
    }
}
appendHoverOverlay(objects, previewWorld, materialTextures, hoveredObjectId, selectedIds);
```

**Choosing `pickEditorObject` vs `pickEditorObjects`:** Use `pickEditorObject` (the single-hit first-result variant in `EditorSelectionSystem.h` line 57). It returns `std::optional<EditorHitResult>` and stops at the closest hit after sorting — identical result to `pickEditorObjects().front()` but the intent is clearer. The performance difference is negligible (full sort still runs internally), but the API intent matches the use case.

### Pattern 3: Duplicate Position Offset (OBJ-02)

**What:** After `duplicateObject` returns new IDs, translate each duplicate by a visible offset. The offset should be small but clearly visible — `(0.5f, 0.0f, 0.0f)` in world space is a safe default (half a unit right).

**Implementation in main.cpp, inside `if (duplicatePressed)` block after line 1750:**
```cpp
const std::uint64_t newId = document.duplicateObject(id);
if (newId != 0) {
    // Apply offset so duplicate is visibly distinct from original
    const glm::mat4 currentWorld = document.worldTransformMatrix(newId);
    const glm::mat4 offsetWorld = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f)) * currentWorld;
    document.applyWorldTransform(newId, offsetWorld);
    duplicated.push_back(newId);
}
```

**Note:** `duplicateObject` already calls `addObject` which calls `ensureObjectNodeId` internally (EditorSceneDocument.cpp:714) — no extra call needed. The `nodeId` collision concern from STATE.md is already resolved.

### Pattern 4: Smooth Camera Framing Animation (D-09, D-10)

**What:** `EditorCamera` is a pure POD struct — it holds no animation bookkeeping. To add smooth lerp, add a sibling struct `EditorCameraAnimation` in `EditorViewportController.h`.

**New struct in `EditorViewportController.h`:**
```cpp
struct EditorCameraAnimation {
    bool active = false;
    EditorCamera target;      // snapshot of where camera should end up
    float progress = 0.0f;    // 0.0 = start, 1.0 = done
    float duration = 0.3f;    // seconds
};
```

**Animation tick in `EditorViewportController.cpp` — new function `tickCameraAnimation`:**
```cpp
// Ease-out cubic: t = 1 - (1 - progress)^3
void tickCameraAnimation(EditorCamera& camera,
                         EditorCameraAnimation& anim,
                         float deltaTime) {
    if (!anim.active) return;
    anim.progress = std::min(anim.progress + deltaTime / anim.duration, 1.0f);
    const float t = 1.0f - std::pow(1.0f - anim.progress, 3.0f);  // ease-out cubic

    camera.yawDegrees   = glm::mix(camera.yawDegrees,   anim.target.yawDegrees,   t);
    camera.pitchDegrees = glm::mix(camera.pitchDegrees, anim.target.pitchDegrees, t);
    camera.position     = glm::mix(camera.position,     anim.target.position,     t);
    camera.orbitPivot   = glm::mix(camera.orbitPivot,   anim.target.orbitPivot,   t);
    camera.orbitDistance = glm::mix(camera.orbitDistance, anim.target.orbitDistance, t);
    camera.orbitPivotValid = anim.target.orbitPivotValid;

    if (anim.progress >= 1.0f) {
        camera = anim.target;
        anim.active = false;
    }
}
```

**Animation start — replace direct `focusEditorCameraOnBounds` calls with:**
```cpp
// New function beginFocusAnimation — computes target camera via existing
// focusEditorCameraOnBounds on a copy, then stores in animation struct
void beginFocusAnimation(EditorCamera& camera,
                         EditorCameraAnimation& anim,
                         const glm::vec3& boundsMin,
                         const glm::vec3& boundsMax) {
    EditorCamera target = camera;           // copy current camera state
    focusEditorCameraOnBounds(target, boundsMin, boundsMax);  // compute target
    anim.target = target;
    anim.progress = 0.0f;
    anim.active = true;
}
```

**Multi-selection bounding box (D-10) — in main.cpp focusPressed handler:**
```cpp
if (focusPressed && !selectedIds.empty()) {
    EditorObjectBounds unionBounds;
    for (const auto id : selectedIds) {
        if (const EditorObjectBounds* b = previewWorld.findObjectBounds(id)) {
            unionBounds.expand(*b);
        } else if (const EditorSceneObject* obj = document.findObject(id)) {
            // Fallback: point-expand at anchor
            unionBounds.expand(editorSceneObjectAnchor(*obj));
        }
    }
    if (unionBounds.valid) {
        beginFocusAnimation(editCamera, cameraAnim, unionBounds.min, unionBounds.max);
    }
    focusPressed = false;
}
```

**Animation tick location:** Call `tickCameraAnimation(editCamera, cameraAnim, deltaTime)` at the top of the camera update block, before `updateEditorFlyCamera`. When animation is active, `updateEditorFlyCamera` still runs — user input during animation will modify the same `camera` fields, which breaks the interpolation. Guard `updateEditorFlyCamera` to skip when `cameraAnim.active` (or allow the user input to cancel the animation by setting `cameraAnim.active = false` on right-click/scroll).

**Easing choice (Claude's Discretion):** Ease-out cubic (`1 - (1-t)^3`) is recommended. It provides a fast initial movement that decelerates into the target, matching the Unity/Blender feel. Slerp for rotation is not needed here because `yawDegrees`/`pitchDegrees` are scalars — linear `glm::mix` on them is equivalent and simpler.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Bounding box union | Manual min/max loop | `EditorObjectBounds::expand(const EditorObjectBounds& other)` | Already implemented in EditorPreviewWorld.cpp:109 |
| Ray-to-AABB intersection | Custom raycast | `pickEditorObject()` from EditorSelectionSystem | Full ray-OBB/AABB/sphere/cylinder with priority sorting already exists |
| Text field detection | Custom focus tracking | `io.WantTextInput` from ImGuiIO | ImGui native; works correctly for all ImGui text inputs |
| Camera target position | Hand-calculate orbit math | `focusEditorCameraOnBounds()` applied to copy then stored as animation target | All the orbit math is already correct in this function |

---

## Common Pitfalls

### Pitfall 1: Escape clearing selection when typing in a text field

**What goes wrong:** Without `!io.WantTextInput`, pressing Escape to confirm/cancel a text input in the inspector also fires the editor's Escape handler, clearing the scene selection.

**Why it happens:** ImGui handles Escape keystrokes for text widgets internally, but `ImGui::IsKeyPressed(ImGuiKey_Escape)` still returns `true` to application code — ImGui does not consume the key event.

**How to avoid:** Wrap ALL single-key shortcut checks in `if (!gameplayPreviewCaptured && !io.WantTextInput)`. This is the fix for D-06.

**Warning signs:** Selection unexpectedly clears after finishing a rename in the inspector.

### Pitfall 2: Delete firing inside ImGui text inputs

**What goes wrong:** Currently line 432 (`if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;`) has no `WantTextInput` guard. Pressing Delete to delete a character in a name field also deletes the selected scene object.

**Why it happens:** Same reason as Escape — ImGui does not consume Delete keystrokes.

**How to avoid:** Add `!io.WantTextInput` guard to the Delete check.

### Pitfall 3: Hover overlay stacking on selected objects

**What goes wrong:** If `appendHoverOverlay` is called without checking `isSelected(selectedIds, hoveredId)`, hovering over an already-selected object produces a stacked blue wireframe on top of the gold selection wireframe — visually noisy and against D-03.

**How to avoid:** The guard `if (hoveredId == 0 || isSelected(selectedIds, hoveredId)) return;` at the top of `appendHoverOverlay`.

### Pitfall 4: Hover raycast running during gizmo drag

**What goes wrong:** When the user is dragging a transform gizmo (`editorGizmoIsHot()` is true), the mouse cursor may be hovering over a different object, causing the hover highlight to flicker onto that object while dragging.

**How to avoid:** Include `editorGizmoIsHot()` in the suppress-hover check (D-04). This is already called in the existing selection click handler at line 1597.

### Pitfall 5: Camera animation fighting user input

**What goes wrong:** If `updateEditorFlyCamera` runs while `cameraAnim.active` is true, user scroll/orbit input modifies `camera.position` and `camera.orbitPivot`, which the animation's `glm::mix` then partially overrides on the next frame — causing jitter.

**How to avoid:** When `cameraAnim.active`, either skip `updateEditorFlyCamera` entirely, or cancel the animation on any user camera input (detect scroll, right-click, middle-click). The simpler approach: cancel animation (`cameraAnim.active = false`) when `editorGizmoIsHot()` is false AND any camera input is detected. Or simply: do not call `updateEditorFlyCamera` while the animation is active.

### Pitfall 6: Duplicate offset applied in local space vs world space

**What goes wrong:** If the offset `(0.5, 0, 0)` is applied to the local-space transform (e.g., modifying `mesh.position` directly) instead of via `applyWorldTransform`, the offset direction changes with parent rotation, producing unexpected results for parented objects.

**How to avoid:** Always use `applyWorldTransform` with a world-space translation matrix. This correctly handles parent-child hierarchies.

### Pitfall 7: previewDirty not set after hover raycast changes hoveredObjectId

**What goes wrong:** The hover highlight only updates when `previewDirty = true` triggers a re-render. If the hover system only triggers a re-render on selection changes, the hover highlight will not track the cursor smoothly.

**How to avoid:** The hover highlight must be computed and rendered every frame regardless of `previewDirty`. Inspect whether the render block is conditional on `previewDirty` or always runs. From the code (line 1311), the render block runs every frame unconditionally — the hover raycast and overlay can be inserted inside that block without setting `previewDirty`.

---

## Code Examples

### Verified `appendSelectionOverlays` overlay pattern (template for hover overlay)

```cpp
// Source: src/editor/render/EditorScenePreviewRenderer.cpp:212-265
// Existing two-pass (ghost + depth-tested) selection overlay:
objects.push_back(RenderObject{
    cube,
    makeModelMatrix(center, size),
    tint * 0.20f,              // ghost pass
    materials.resolve("metal_default"),
    true,   // wireframe
    true,   // ignoreDepth = true (ghost)
    true,   // unlit
    primary ? 2.0f : 1.5f
});
objects.push_back(RenderObject{
    cube,
    makeModelMatrix(center, size),
    tint,                      // primary pass
    materials.resolve("metal_default"),
    true,   // wireframe
    false,  // ignoreDepth = false (depth-tested)
    true,   // unlit
    primary ? 4.0f : 2.5f
});

// Hover overlay: single pass, depth-tested only (D-02)
// tint = glm::vec3(0.55f, 0.85f, 1.00f), lineWidth = 2.0f
```

### Verified `pickEditorObject` single-hit API

```cpp
// Source: src/editor/scene/EditorSelectionSystem.h:57
std::optional<EditorHitResult> pickEditorObject(
    const std::vector<EditorSelectionHandle>& handles,
    const EditorRay& ray);
// Returns first (closest, priority-sorted) hit, or nullopt if miss.
```

### Verified `EditorObjectBounds::expand` for union bounds

```cpp
// Source: src/editor/scene/EditorPreviewWorld.h:21
void expand(const EditorObjectBounds& other);
// Expands this bounds to contain `other`. Use for union across selectedIds.
```

### Verified `applyWorldTransform` for post-duplicate offset

```cpp
// Source: src/editor/scene/EditorSceneDocument.h:90
bool applyWorldTransform(std::uint64_t id, const glm::mat4& worldMatrix);
// Decomposes worldMatrix into local coords (handling parent hierarchy).
// Use: applyWorldTransform(newId, glm::translate(identity, offset) * worldTransformMatrix(newId));
```

### Verified `focusEditorCameraOnBounds` current implementation

```cpp
// Source: src/editor/viewport/EditorViewportController.cpp:181-190
void focusEditorCameraOnBounds(EditorCamera& camera,
                               const glm::vec3& boundsMin,
                               const glm::vec3& boundsMax) {
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 extents = glm::max((boundsMax - boundsMin) * 0.5f, glm::vec3(0.25f));
    const float radius = glm::length(extents);
    const glm::vec3 forward = editorCameraForward(camera);
    camera.position = center - forward * std::max(radius * 2.4f, 3.0f);
    focusEditorCameraOnPoint(camera, center);
}
// For animation: apply this to a COPY of editCamera to compute target,
// then store target in EditorCameraAnimation.target.
```

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|------------------|-------|
| F key instant snap (line 1708) | Smooth lerp via `EditorCameraAnimation` state | New state struct needed |
| F key single-object only | F key uses union bounds of all selected objects | Multi-select framing per D-10 |
| Escape only clears placement/gizmo | Escape also clears `selectedIds` | Add to Escape handler |
| Delete/F unguarded from text inputs | Delete/F/Escape guarded with `!io.WantTextInput` | Fix existing checks |
| No hover highlight | Hover highlight on first-hit raycast per frame | New per-frame raycast + overlay |
| Duplicate: no position offset | Duplicate: world-space `(0.5, 0, 0)` offset via `applyWorldTransform` | Post-duplicate transform |

---

## Open Questions

1. **Animation cancellation on user input**
   - What we know: `updateEditorFlyCamera` is called when viewport is active; it modifies `camera.position` and `camera.orbitPivot`
   - What's unclear: whether to (a) skip fly camera entirely during animation, or (b) cancel animation when any camera input is detected
   - Recommendation: Cancel animation (set `active = false`) when any scroll wheel, right-click, or alt+drag input is detected during the animation tick. This is the most predictable behavior — user can always interrupt a framing operation.

2. **Hover suppression during orbit/pan: right-click only vs. alt-drag**
   - What we know: D-04 says suppress during "right-mouse orbit/pan". In `EditorViewportController.cpp`, orbit is alt+left-drag, pan is middle-drag, dolly is alt+right-drag, and free-fly is right-drag.
   - What's unclear: D-04 says "right-mouse orbit/pan" — but orbit is actually alt+left. The intent is to suppress hover during any active camera manipulation.
   - Recommendation: Suppress hover when `glfwGetMouseButton(GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS || (io.KeyAlt && glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) || glfwGetMouseButton(GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS`.

---

## Environment Availability

Step 2.6: SKIPPED — this phase is entirely code changes to the existing C++ editor. No external tools, services, or runtimes beyond the existing build system are required.

---

## Sources

### Primary (HIGH confidence)
- `src/editor/render/EditorScenePreviewRenderer.cpp` — verified RenderObject overlay pattern, appendSelectionOverlays exact field values
- `src/editor/viewport/EditorViewportController.cpp` — verified focusEditorCameraOnBounds/Point, EditorCamera struct (no animation state)
- `src/editor/viewport/EditorViewportInteraction.h` — verified pickEditorObject signature
- `src/editor/scene/EditorSelectionSystem.h` — verified EditorHitResult, pickEditorObject, pickEditorObjects
- `src/editor/scene/EditorPreviewWorld.h` — verified EditorObjectBounds::expand(EditorObjectBounds&)
- `src/editor/scene/EditorSceneDocument.cpp:602-608` — verified duplicateObject calls addObject which calls ensureObjectNodeId
- `src/engine/rendering/geometry/Renderer.h` — verified RenderObject struct fields (wireframe, ignoreDepth, unlit, lineWidth)
- `apps/level_editor/main.cpp:426-461` — verified existing shortcut block and gaps
- `apps/level_editor/main.cpp:1708-1717` — verified current F-key instant-snap implementation
- `apps/level_editor/main.cpp:1743-1764` — verified duplicate handler (no offset applied)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all APIs read directly from source
- Architecture patterns: HIGH — all code paths traced through actual source files
- Pitfalls: HIGH — identified from direct code inspection (missing `!io.WantTextInput` guards confirmed line-by-line)

**Research date:** 2026-04-01
**Valid until:** 2026-05-01 (stable C++ codebase; expires if EditorCamera struct is refactored)
