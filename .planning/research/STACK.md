# Stack Research

**Domain:** Custom C++ Level Editor — Professional Object Manipulation UX
**Researched:** 2026-04-01
**Confidence:** HIGH (all conclusions verified against existing codebase + official sources)

---

## Executive Answer

**No new library dependencies are required.** Every capability needed for delete, duplicate, add-mesh, and improved selection is already available in the current vendored stack. The work is entirely integration and engine-side rendering changes.

---

## Existing Stack Components That Do The Work

These are already present. This section maps each milestone feature to the existing tool that implements it.

### Feature: Delete selected objects (Delete key)

**Tool: Dear ImGui v1.92.6-docking** (`imgui` FetchContent, `GIT_TAG v1.92.6-docking`)

Use `ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteFocused)` inside the viewport window. The `RouteFocused` flag ensures Delete is not consumed when an `InputText` widget is active — the key ownership system introduced in 1.87 and backfilled to `InputText` in 1.92.x (issue #8048) handles this correctly. No raw GLFW key polling needed.

The actual deletion calls `EditorSceneDocument::eraseObjects()` which already exists and handles subtree cleanup. Wrap with `EditorDocumentStateCommand` for undo.

### Feature: Duplicate selected objects (Ctrl+D)

**Tool: Dear ImGui v1.92.6-docking** — same `ImGui::Shortcut()` API, same routing.

`EditorSceneDocument::duplicateObject()` already exists. The only new work is wiring the shortcut → document call → command stack push.

### Feature: Add meshes via asset browser drag or picker

**Tool: EditorPlacementState + existing drag-drop** — already implemented in `EditorViewportInteraction.cpp` (`commitPlacement`, `appendPlacementGhost`). The asset browser already emits drag payloads (`emitPlacementDragSource`). The gap is a direct "add at cursor" action without requiring drag; this is pure ImGui button/combo UI work, no new library.

**Tool: MeshLibrary** — `sortedMeshNames()` is already available in `LevelEditorCore`. A mesh picker combo reads from this directly.

### Feature: Improved selection overlay (remove distracting overlay behind other meshes)

**Tool: OpenGL 4.1 stencil buffer** — already available; the engine uses `glad/gl.h` with OpenGL 4.1 Core Profile.

The current selection overlay in `appendSelectionOverlays()` renders a scaled wireframe cube with `ignoreDepth = true`, which draws on top of occluding geometry. The fix is a two-pass stencil technique:

1. Pass 1 — render selected mesh normally, write 1 to stencil where fragments land
2. Pass 2 — render slightly-scaled mesh with `GL_NOTEQUAL` stencil test, flat color/emissive shader, no depth write

This produces a pixel-accurate outline that is occluded by intervening geometry. No library. Pure GLSL + GL state calls. The outline shader is a new `assets/shaders/engine/selection_outline.frag` file.

The `SceneRenderPipeline` needs a post-scene outline pass that reads the stencil buffer. This is an engine rendering change, not a dependency change.

### Feature: Multi-object selection (Shift+click, Ctrl+click, box-select)

**Tool: Dear ImGui v1.92.6-docking** — `BeginMultiSelect` / `EndMultiSelect` API shipped in 1.91.0 (Jul 2024). Already present in the vendored version.

`ImGuiSelectionBasicStorage` is available but is designed for list widgets. For the 3D viewport the project already manages `std::vector<std::uint64_t> selectedIds` directly. Use `ImGuiMultiSelectIO` requests from `BeginMultiSelect` only for the **outliner panel**. Viewport selection (ray-cast based) is custom and does not use BeginMultiSelect — the existing `toggleSelection()` / `applySelectionHit()` functions in `EditorViewportInteraction.h` already handle additive selection correctly.

### Feature: Transform gizmo for move/translate

**Tool: ImGuizmo v1.92.5 WIP** — vendored in `external/ImGuizmo/`, last upstream commit Dec 27, 2025. `ImGuizmo::Manipulate` is already wired in `EditorViewportController.cpp`. The gizmo is operational; the gap is ensuring move operations push to `EditorCommandStack` on drag-end (use `ImGuizmo::IsUsing()` / transition to not-using as the commit signal — already tracked via `gizmoCommand` in the editor state).

---

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| A new outline/selection library | Zero libraries exist for this that integrate with a custom pipeline | OpenGL stencil buffer (native, zero cost) |
| Raw `glfwGetKey()` polling for Delete/Ctrl+D | Bypasses ImGui's key ownership system; causes Delete to fire while typing in InputText | `ImGui::Shortcut()` with `ImGuiInputFlags_RouteFocused` |
| `ImGuiSelectionBasicStorage` in the 3D viewport | Designed for list widgets, not ray-cast 3D selection; adds indirection layer over `selectedIds` which is already well-managed | Existing `std::vector<std::uint64_t> selectedIds` + `toggleSelection()` |
| Upgrading ImGui to v2.x or later | No v2.x exists; 1.92.6-docking is current; upgrading mid-milestone creates risk with ImGuizmo compatibility (ImGuizmo version-matches to imgui internals) | Stay on v1.92.6-docking |
| A separate picking library (e.g., color-ID FBO) | The project already uses AABB/oriented-box hit testing in `pickEditorObject()`; color-ID picking would require an extra render pass and readback, more overhead for no benefit at this object count | Keep existing ray-cast picking |
| Replacing ImGuizmo with a newer alternative (e.g., imGuIZMO.quat, gizmo3d) | ImGuizmo is already integrated and working; alternatives would require re-integration and testing | Keep ImGuizmo at current vendored v1.92.5 WIP |

---

## Version Compatibility Notes

| Component | Current Version | Status | Notes |
|-----------|-----------------|--------|-------|
| Dear ImGui | v1.92.6-docking | Current | `Shortcut()` API + key ownership + `BeginMultiSelect` all present. `InputText` Delete ownership bug (#8048) fixed. |
| ImGuizmo | v1.92.5 WIP (vendored master ~Dec 2025) | Current | Tracks ImGui version closely; compatible with 1.92.6. Rotation gizmo rendering fix landed Nov–Dec 2025. |
| OpenGL | 4.1 Core Profile | Pinned (macOS ceiling) | Stencil buffer fully supported in 4.1. `glStencilFunc`, `glStencilOp`, `glStencilMask` available as needed. |
| EnTT | v3.16.0 | Current | ECS queries in editor preview world are unchanged; no impact from this milestone. |

---

## Integration Points

The following are the exact files where new code attaches to existing systems:

| New capability | File to modify | Existing hook |
|----------------|---------------|---------------|
| Delete shortcut | `src/editor/viewport/EditorViewportInteraction.cpp` | Add `ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteFocused)` check, call `eraseObjects()`, push `EditorDocumentStateCommand` |
| Ctrl+D duplicate | `src/editor/viewport/EditorViewportInteraction.cpp` | Same pattern, call `duplicateObject()` per selected ID |
| Stencil outline pass | `src/engine/rendering/SceneRenderPipeline.cpp` + new `assets/shaders/engine/selection_outline.vert/.frag` | New post-scene pass after main scene draw, before post-process |
| Stencil overlay suppress | `src/editor/render/EditorScenePreviewRenderer.cpp` | Remove `appendSelectionOverlays()` call; replace with stencil IDs passed to pipeline |
| Mesh picker UI | `src/editor/ui/EditorAssetBrowserPanel.cpp` or new inline panel | `sortedMeshNames()` + `beginPlacement()` |

---

## Sources

- Dear ImGui GitHub: `v1.92.6-docking` tag confirmed current — https://github.com/ocornut/imgui — HIGH confidence
- ImGui Multi-Select API wiki: shipped in 1.91.0 (Jul 2024), `BeginMultiSelect` + `ImGuiSelectionBasicStorage` documented — https://github.com/ocornut/imgui/wiki/Multi-Select — HIGH confidence
- ImGui issue #8048: `InputText` Delete key ownership fixed, `SetKeyOwner(ImGuiKey_Delete, id)` — https://github.com/ocornut/imgui/issues/8048 — HIGH confidence
- ImGuizmo GitHub master: last commit Dec 27, 2025 (LightRig Editor), version string v1.92.5 WIP, compatible with Dear ImGui 1.92.x — https://github.com/CedricGuillemet/ImGuizmo/commits/master — HIGH confidence
- LearnOpenGL Stencil Testing: two-pass outline technique for object selection overlays — https://learnopengl.com/Advanced-OpenGL/Stencil-testing — HIGH confidence
- Project codebase: `external/ImGuizmo/ImGuizmo.h` version string confirmed v1.92.5 WIP; `CMakeLists.txt` FetchContent confirmed `imgui v1.92.6-docking`, `entt v3.16.0`, `JoltPhysics v5.4.0`, `glad v2.0.8` — VERIFIED directly

---

*Stack research for: v1.1 Editor UX — object manipulation (delete, duplicate, add mesh, improved selection)*
*Researched: 2026-04-01*
