# Drag-and-Drop Material Assignment

**Date:** 2026-04-11
**Status:** Draft

## Summary

Drag a material from the asset browser or inspector onto a mesh in the viewport to assign it — matching Unity's drag-and-drop material workflow. A live preview shows the material on the mesh while hovering before the drop commits.

## Requirements

1. **Drag sources**: Asset browser material entries and mesh inspector material combo entries emit a new `EDITOR_MATERIAL` drag payload.
2. **Drop target**: The scene viewport accepts `EDITOR_MATERIAL` payloads. On drop, the material is assigned to the single mesh under the cursor.
3. **Live preview**: While hovering over a valid mesh during a material drag, the mesh renders with the dragged material. No document mutation — purely a render-time override that clears when the cursor moves off the mesh or the drag ends.
4. **Undo/redo**: The drop pushes an `"Assign Material"` command using the existing `captureState()` / `pushDocumentStateCommand()` pattern.
5. **Single-mesh targeting**: The drop always targets the one mesh under the cursor, regardless of the current selection.

## Design

### Payload

New payload type `"EDITOR_MATERIAL"` with a fixed-size struct:

```cpp
struct EditorMaterialDragPayload {
    char materialId[64]{};
};
```

Defined in `LevelEditorUi.h` alongside the existing `EditorDragPayload`.

### Shared Drag Source Helper

New function in `EditorPanelUtils.h/cpp`:

```cpp
void emitMaterialDragSource(const std::string& materialId);
```

Mirrors the existing `emitPlacementDragSource()` pattern:
- Calls `ImGui::BeginDragDropSource()`
- Copies `materialId` into an `EditorMaterialDragPayload`
- Sets payload with `ImGui::SetDragDropPayload("EDITOR_MATERIAL", ...)`
- Shows tooltip text with the material name
- Calls `ImGui::EndDragDropSource()`

### Drag Source: Asset Browser

**File:** `EditorAssetBrowserPanel.cpp`

After the existing click handler on material `Selectable` entries (~line 475), call `emitMaterialDragSource(declaredId)`. The existing click-to-select behavior is preserved — ImGui distinguishes click from drag initiation.

### Drag Source: Mesh Inspector

**File:** `MeshInspector.cpp`

Each `Selectable` entry inside the material combo box (~line 50) gets an `emitMaterialDragSource(materialId)` call. This lets users drag a material directly out of the dropdown.

### Viewport Drop Target

**File:** `apps/level_editor/main.cpp`

The existing `BeginDragDropTarget` block (~line 2015) gets a second `AcceptDragDropPayload("EDITOR_MATERIAL")` call alongside the existing `EDITOR_PLACE` one.

**Hover (before delivery):**
1. Read the pending `EDITOR_MATERIAL` payload from `ImGui::GetDragDropPayload()`
2. Ray-pick the mesh under the cursor using `pickEditorObject()` with the existing selection handles
3. Filter: only accept `EditorSceneObjectKind::Mesh` hits
4. Store the transient override pair `(hoveredObjectId, materialId)` for the live preview

**Drop (delivery):**
1. `document.captureState()` → before state
2. Find the mesh object by picked ID in the document
3. If the mesh already has this material → no-op (skip)
4. Set `mesh.materialId = droppedMaterialId`
5. `document.markSceneDirty()`
6. `commandStack.pushDocumentStateCommand("Assign Material", beforeState, document.captureState(), document)`

### Live Preview

**Integration point:** `collectRenderObjects()` in `EditorScenePreviewRenderer.cpp` (~line 103).

This function iterates ECS entities with `TransformComponent` + `MeshComponent` and builds `RenderObject` entries. Each entity maps back to a document object ID via `world.ownerMap()`.

The transient material override is passed as an optional parameter (or a small struct) to `collectRenderObjects()`:

```cpp
struct MaterialDragPreview {
    std::uint64_t objectId = 0;     // document object being hovered
    std::string_view materialId;     // material to preview
};
```

Inside the loop, when a mesh entity's owner matches `objectId`, `materials.resolve(preview.materialId)` is used instead of `materials.resolve(mesh.materialId)`.

When no drag is active or the cursor isn't over a mesh, the preview struct is empty (objectId = 0) and the code path is a single branch-not-taken.

### Viewport Dirty Gate

The viewport dirty gate (~line 1691) already checks for active `EDITOR_PLACE` drags to force redraws. Add the same check for `EDITOR_MATERIAL`:

```cpp
|| (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_MATERIAL"))
```

This ensures the viewport keeps redrawing while the material drag hover is active, so the live preview updates as the cursor moves across meshes.

### Edge Cases

- **Empty space / non-mesh target**: No preview, no drop. ImGui naturally ignores the payload when no target accepts it.
- **Same material already assigned**: Drop is a no-op — no undo command pushed.
- **Invalid material ID**: Validated against `ContentRegistry` on drop. If the material was removed mid-drag (hot-reload), the drop is silently ignored.
- **Drag cancellation** (release outside viewport / Escape): ImGui cancels the drag, transient override clears, mesh reverts. No state change.

## Files Modified

| File | Change |
|------|--------|
| `src/editor/ui/LevelEditorUi.h` | Add `EditorMaterialDragPayload` struct |
| `src/editor/ui/EditorPanelUtils.h` | Declare `emitMaterialDragSource()` |
| `src/editor/ui/EditorPanelUtils.cpp` | Implement `emitMaterialDragSource()` |
| `src/editor/ui/EditorAssetBrowserPanel.cpp` | Call `emitMaterialDragSource()` on material entries |
| `src/editor/ui/inspectors/MeshInspector.cpp` | Call `emitMaterialDragSource()` on combo entries |
| `src/editor/render/EditorScenePreviewRenderer.h` | Add `MaterialDragPreview` struct, update `collectRenderObjects()` signature |
| `src/editor/render/EditorScenePreviewRenderer.cpp` | Implement material override in `collectRenderObjects()` |
| `apps/level_editor/main.cpp` | Add `EDITOR_MATERIAL` to dirty gate, hover ray-pick, drop handler, pass preview to render |
