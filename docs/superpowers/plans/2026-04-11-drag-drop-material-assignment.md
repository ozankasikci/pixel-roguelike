# Drag-and-Drop Material Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drag a material from the asset browser or inspector onto a mesh in the viewport to assign it, with live preview while hovering.

**Architecture:** New `EDITOR_MATERIAL` drag payload type. Both asset browser and mesh inspector emit it via a shared helper. The viewport accepts it as a second drop target alongside `EDITOR_PLACE`. During hover, `collectRenderObjects()` receives a transient material override for the hovered mesh — no document mutation until actual drop.

**Tech Stack:** C++20, ImGui drag-drop API, existing ray-picking infrastructure, existing undo/redo command system.

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `src/editor/ui/LevelEditorUi.h` | Modify | Add `EditorMaterialDragPayload` struct, declare `emitMaterialDragSource()` |
| `src/editor/ui/EditorPanelUtils.cpp` | Modify | Implement `emitMaterialDragSource()` |
| `src/editor/ui/EditorAssetBrowserPanel.cpp` | Modify | Emit material drag source on material entries |
| `src/editor/ui/inspectors/MeshInspector.cpp` | Modify | Emit material drag source on combo entries |
| `src/editor/render/EditorScenePreviewRenderer.h` | Modify | Add `MaterialDragPreview` struct, update `collectRenderObjects()` signature |
| `src/editor/render/EditorScenePreviewRenderer.cpp` | Modify | Apply transient material override in render object collection |
| `apps/level_editor/main.cpp` | Modify | Dirty gate, hover ray-pick for material drag, drop handler, pass preview to render |

---

### Task 1: Add payload struct and drag source helper

**Files:**
- Modify: `src/editor/ui/LevelEditorUi.h:162-239`
- Modify: `src/editor/ui/EditorPanelUtils.cpp:272-285`

- [ ] **Step 1: Add `EditorMaterialDragPayload` to `LevelEditorUi.h`**

In `src/editor/ui/LevelEditorUi.h`, after the existing `EditorDragPayload` struct (line 166), add:

```cpp
struct EditorMaterialDragPayload {
    char materialId[64]{};
};
```

- [ ] **Step 2: Declare `emitMaterialDragSource()` in `LevelEditorUi.h`**

After the `emitPlacementDragSource` declaration (line 239), add:

```cpp
void emitMaterialDragSource(const std::string& materialId);
```

- [ ] **Step 3: Implement `emitMaterialDragSource()` in `EditorPanelUtils.cpp`**

After the existing `emitPlacementDragSource` function (after line 285), add:

```cpp
void emitMaterialDragSource(const std::string& materialId) {
    if (!ImGui::BeginDragDropSource()) {
        return;
    }
    EditorMaterialDragPayload payload;
    copyPayloadString(payload.materialId, materialId);
    ImGui::SetDragDropPayload("EDITOR_MATERIAL", &payload, sizeof(payload));
    ImGui::TextUnformatted(materialId.c_str());
    ImGui::EndDragDropSource();
}
```

Note: `copyPayloadString` is a file-local helper already defined at line 18 of `EditorPanelUtils.cpp`. It uses `std::snprintf` to safely copy a string into a `char[64]` buffer.

- [ ] **Step 4: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds. The new struct and function are defined but not yet called.

- [ ] **Step 5: Commit**

```bash
git add src/editor/ui/LevelEditorUi.h src/editor/ui/EditorPanelUtils.cpp
git commit -m "Add EditorMaterialDragPayload and emitMaterialDragSource helper"
```

---

### Task 2: Emit material drag from asset browser

**Files:**
- Modify: `src/editor/ui/EditorAssetBrowserPanel.cpp:509-513`

- [ ] **Step 1: Add material drag source after existing placement drag sources**

In `src/editor/ui/EditorAssetBrowserPanel.cpp`, the block at lines 509-513 currently emits placement drag sources for meshes and archetypes:

```cpp
        if (meshPlaceable) {
            emitPlacementDragSource(EditorPlacementKind::Mesh, meshId, ui.selectedMaterialId);
        } else if (node.kind == EditorAssetBrowserKind::Prefab && archetypeKnown) {
            emitPlacementDragSource(EditorPlacementKind::Archetype, declaredId);
        }
```

Add a new `else if` branch for materials after the archetype branch (after line 513):

```cpp
        if (meshPlaceable) {
            emitPlacementDragSource(EditorPlacementKind::Mesh, meshId, ui.selectedMaterialId);
        } else if (node.kind == EditorAssetBrowserKind::Prefab && archetypeKnown) {
            emitPlacementDragSource(EditorPlacementKind::Archetype, declaredId);
        } else if (materialKnown) {
            emitMaterialDragSource(declaredId);
        }
```

The `materialKnown` bool is already computed at line 490: `!declaredId.empty() && containsString(materialIds, declaredId)`.

- [ ] **Step 2: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds.

- [ ] **Step 3: Manual test — drag from asset browser**

Launch the editor, open a scene, find a material in the asset browser. Click and drag it — you should see the material name as a tooltip following the cursor. Releasing anywhere does nothing yet (no drop target).

- [ ] **Step 4: Commit**

```bash
git add src/editor/ui/EditorAssetBrowserPanel.cpp
git commit -m "Emit EDITOR_MATERIAL drag source from asset browser material entries"
```

---

### Task 3: Emit material drag from mesh inspector combo

**Files:**
- Modify: `src/editor/ui/inspectors/MeshInspector.cpp:48-62`

- [ ] **Step 1: Add drag source to each combo entry**

In `src/editor/ui/inspectors/MeshInspector.cpp`, inside the material combo box loop (lines 48-62), add `emitMaterialDragSource(materialId)` after each `Selectable`. The current code:

```cpp
        if (ImGui::BeginCombo("##value", currentMaterialLabel.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == currentMaterialLabel;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    if (mesh.materialId != materialId) {
                        const EditorSceneDocumentState before = document.captureState();
                        mesh.materialId = materialId;
                        document.markSceneDirty();
                        commandStack.pushDocumentStateCommand("Change Mesh Material", before, document.captureState(), document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
```

Add the drag source call after the `SetItemDefaultFocus` block, before the closing of the `for` loop:

```cpp
        if (ImGui::BeginCombo("##value", currentMaterialLabel.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == currentMaterialLabel;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    if (mesh.materialId != materialId) {
                        const EditorSceneDocumentState before = document.captureState();
                        mesh.materialId = materialId;
                        document.markSceneDirty();
                        commandStack.pushDocumentStateCommand("Change Mesh Material", before, document.captureState(), document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
                emitMaterialDragSource(materialId);
            }
            ImGui::EndCombo();
        }
```

- [ ] **Step 2: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds.

- [ ] **Step 3: Manual test — drag from inspector combo**

Launch the editor, select a mesh, open the Material Id combo in the inspector. Click and drag an entry — you should see the material name tooltip. Releasing anywhere does nothing yet.

- [ ] **Step 4: Commit**

```bash
git add src/editor/ui/inspectors/MeshInspector.cpp
git commit -m "Emit EDITOR_MATERIAL drag source from mesh inspector material combo"
```

---

### Task 4: Add `MaterialDragPreview` and update `collectRenderObjects` signature

**Files:**
- Modify: `src/editor/render/EditorScenePreviewRenderer.h:24-26`
- Modify: `src/editor/render/EditorScenePreviewRenderer.cpp:103-125`
- Modify: `apps/level_editor/main.cpp:1717`

- [ ] **Step 1: Add `MaterialDragPreview` struct to `EditorScenePreviewRenderer.h`**

Before the `collectRenderObjects` declaration (line 24), add:

```cpp
struct MaterialDragPreview {
    std::uint64_t objectId = 0;
    std::string_view materialId;
};
```

- [ ] **Step 2: Update `collectRenderObjects` declaration**

Change the declaration at line 24-26 from:

```cpp
std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds);
```

to:

```cpp
std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds,
                                               const MaterialDragPreview& dragPreview = {});
```

- [ ] **Step 3: Update `collectRenderObjects` implementation**

In `src/editor/render/EditorScenePreviewRenderer.cpp`, change the function signature at line 103-105:

```cpp
std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds,
                                               const MaterialDragPreview& dragPreview) {
```

Then update the material resolve inside the loop (lines 113-122). Replace:

```cpp
        glm::vec3 tint = mesh.tint;
        auto ownerIt = world.ownerMap().find(entity);
        if (ownerIt != world.ownerMap().end() && isSelected(selectedIds, ownerIt->second)) {
            tint = glm::min(tint * 1.20f + glm::vec3(0.08f, 0.08f, 0.02f), glm::vec3(1.4f));
        }
        objects.push_back(RenderObject{
            mesh.mesh,
            mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix(),
            tint,
            materials.resolve(mesh.materialId)
        });
```

with:

```cpp
        glm::vec3 tint = mesh.tint;
        auto ownerIt = world.ownerMap().find(entity);
        const std::uint64_t ownerId = (ownerIt != world.ownerMap().end()) ? ownerIt->second : 0;
        if (ownerId != 0 && isSelected(selectedIds, ownerId)) {
            tint = glm::min(tint * 1.20f + glm::vec3(0.08f, 0.08f, 0.02f), glm::vec3(1.4f));
        }
        const bool useDragMaterial = (dragPreview.objectId != 0
                                      && ownerId == dragPreview.objectId
                                      && !dragPreview.materialId.empty());
        const auto& material = useDragMaterial
            ? materials.resolve(dragPreview.materialId)
            : materials.resolve(mesh.materialId);
        objects.push_back(RenderObject{
            mesh.mesh,
            mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix(),
            tint,
            material
        });
```

- [ ] **Step 4: Update the caller in `main.cpp`**

The call at `apps/level_editor/main.cpp:1717` already works as-is because the new parameter has a default value. No change needed for this step — the default `MaterialDragPreview{}` (objectId=0) causes no override. We will pass the actual preview in Task 6.

- [ ] **Step 5: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds. Behavior is unchanged — the default preview has objectId=0 so the override branch is never taken.

- [ ] **Step 6: Commit**

```bash
git add src/editor/render/EditorScenePreviewRenderer.h src/editor/render/EditorScenePreviewRenderer.cpp
git commit -m "Add MaterialDragPreview parameter to collectRenderObjects"
```

---

### Task 5: Add viewport dirty gate for material drag

**Files:**
- Modify: `apps/level_editor/main.cpp:1691`

- [ ] **Step 1: Add `EDITOR_MATERIAL` check to dirty gate**

In `apps/level_editor/main.cpp`, the viewport dirty gate at line 1691 checks for active `EDITOR_PLACE` drags:

```cpp
            || (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_PLACE"))
```

Add an `EDITOR_MATERIAL` check on the next line:

```cpp
            || (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_PLACE"))
            || (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_MATERIAL"))
```

This ensures the viewport keeps redrawing while a material drag is active, so the live preview updates as the cursor moves across meshes.

- [ ] **Step 2: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add apps/level_editor/main.cpp
git commit -m "Add EDITOR_MATERIAL drag to viewport dirty gate for live preview"
```

---

### Task 6: Wire up viewport hover ray-pick, live preview, and drop handler

**Files:**
- Modify: `apps/level_editor/main.cpp:1717,1749,2014-2036`

This is the main integration task. It connects the drag payload to the ray-pick system, feeds the `MaterialDragPreview` into `collectRenderObjects`, and handles the actual drop.

- [ ] **Step 1: Add material drag detection and pass preview to `collectRenderObjects`**

In `apps/level_editor/main.cpp`, inside the `viewportDirty` block, add material drag detection **before** the `collectRenderObjects` call. `viewportSelectionHandles` is built at line 1659 and `inverseViewProjection` at line 1712 — both available here.

Change line 1717 from:

```cpp
            std::vector<RenderObject> objects = collectRenderObjects(previewWorld, materialTextures, selectedIds);
```

to:

```cpp
            // Detect material drag for live preview
            MaterialDragPreview materialDragPreview;
            if (renderViewportState.hovered) {
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                    payload != nullptr && payload->IsDataType("EDITOR_MATERIAL") && payload->DataSize == sizeof(EditorMaterialDragPayload)) {
                    const auto& matPayload = *static_cast<const EditorMaterialDragPayload*>(payload->Data);
                    const EditorRay ray = buildEditorRay(
                        inverseViewProjection,
                        glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                        glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                        glm::vec2(io.MousePos.x, io.MousePos.y));
                    if (const auto hit = pickEditorObject(viewportSelectionHandles, ray)) {
                        if (hit->objectKind == EditorSceneObjectKind::Mesh) {
                            materialDragPreview.objectId = hit->objectId;
                            materialDragPreview.materialId = matPayload.materialId;
                        }
                    }
                }
            }

            std::vector<RenderObject> objects = collectRenderObjects(previewWorld, materialTextures, selectedIds, materialDragPreview);
```

`EditorScenePreviewRenderer.h` (which declares `MaterialDragPreview`) is already included in main.cpp — `collectRenderObjects` is called from this file.

- [ ] **Step 2: Add drop handler for `EDITOR_MATERIAL`**

In `apps/level_editor/main.cpp`, the `BeginDragDropTarget` block at lines 2014-2036 currently only accepts `EDITOR_PLACE`. Add a second `AcceptDragDropPayload` for `EDITOR_MATERIAL` **inside the same `BeginDragDropTarget` / `EndDragDropTarget` pair**, after the existing `EDITOR_PLACE` handler (after line 2034) and before `EndDragDropTarget` (line 2035):

Change from:

```cpp
            if (!ui.playPreview && !startupViewportHandoffActive) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_PLACE", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                        if (payload->Delivery && placementPoint.has_value() && payload->DataSize == sizeof(EditorDragPayload)) {
                            const EditorPlacementState droppedState = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                            const EditorSceneDocumentState beforeState = document.captureState();
                            const auto placedId = commitPlacement(document, droppedState, *placementPoint, content, editCamera);
                            commandStack.pushDocumentStateCommand(
                                "Place Object",
                                beforeState,
                                document.captureState(),
                                document);
                            selectionPicker.clear();
                            if (placedId.has_value()) {
                                selectedIds = { *placedId };
                                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                                ui.scrollToSelection = true;
                            }
                            previewDirty = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
```

to:

```cpp
            if (!ui.playPreview && !startupViewportHandoffActive) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_PLACE", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                        if (payload->Delivery && placementPoint.has_value() && payload->DataSize == sizeof(EditorDragPayload)) {
                            const EditorPlacementState droppedState = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                            const EditorSceneDocumentState beforeState = document.captureState();
                            const auto placedId = commitPlacement(document, droppedState, *placementPoint, content, editCamera);
                            commandStack.pushDocumentStateCommand(
                                "Place Object",
                                beforeState,
                                document.captureState(),
                                document);
                            selectionPicker.clear();
                            if (placedId.has_value()) {
                                selectedIds = { *placedId };
                                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                                ui.scrollToSelection = true;
                            }
                            previewDirty = true;
                        }
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_MATERIAL")) {
                        if (payload->DataSize == sizeof(EditorMaterialDragPayload)) {
                            const auto& matPayload = *static_cast<const EditorMaterialDragPayload*>(payload->Data);
                            const std::string droppedMaterialId = matPayload.materialId;
                            const EditorRay ray = buildEditorRay(
                                inverseViewProjection,
                                glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                                glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                                glm::vec2(io.MousePos.x, io.MousePos.y));
                            if (const auto hit = pickEditorObject(viewportSelectionHandles, ray)) {
                                if (hit->objectKind == EditorSceneObjectKind::Mesh) {
                                    if (EditorSceneObject* object = document.findObject(hit->objectId)) {
                                        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
                                        if (mesh.materialId != droppedMaterialId) {
                                            const EditorSceneDocumentState beforeState = document.captureState();
                                            mesh.materialId = droppedMaterialId;
                                            document.markSceneDirty();
                                            commandStack.pushDocumentStateCommand(
                                                "Assign Material",
                                                beforeState,
                                                document.captureState(),
                                                document);
                                            previewDirty = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
```

Key details:
- `document.findObject(hit->objectId)` returns `EditorSceneObject*` — defined in `EditorSceneDocument.h:74`
- `std::get<LevelMeshPlacement>(object->payload)` extracts the mesh data — same pattern used in `applyMaterialToMeshes()` at `EditorPanelUtils.cpp:155`
- The `EDITOR_MATERIAL` accept does NOT use `ImGuiDragDropFlags_AcceptBeforeDelivery` — it only fires on actual drop, not hover
- If `mesh.materialId` already equals `droppedMaterialId`, the drop is a no-op (no command pushed)

- [ ] **Step 3: Build to verify compilation**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -20`

Expected: Build succeeds.

- [ ] **Step 4: Manual test — full workflow**

Launch the editor, open a scene with multiple meshes that have different materials:

1. **Asset browser drag-drop**: Drag a material from the asset browser onto a mesh in the viewport. The mesh should show the dragged material as a live preview while hovering. On drop, the material is assigned. Verify in the inspector that the material ID changed.

2. **Inspector combo drag-drop**: Select a mesh, open the Material Id combo, drag a material entry onto a different mesh in the viewport. Same behavior — live preview + assignment.

3. **Undo/redo**: After assigning via drag-drop, press Ctrl+Z. The material should revert. Press Ctrl+Y — it should re-apply.

4. **Same material no-op**: Drag a material onto a mesh that already has that material. Nothing should happen (no undo entry created).

5. **Invalid target**: Drag a material over empty space, lights, colliders. No preview should appear, and dropping does nothing.

6. **Drag cancellation**: Start dragging a material, then release outside the viewport or press Escape. The mesh should revert to its original material with no state change.

- [ ] **Step 5: Commit**

```bash
git add apps/level_editor/main.cpp
git commit -m "Wire up viewport material drop target with live preview and undo"
```
