# Architecture Research

**Domain:** Custom C++ game engine level editor — UX feature integration
**Researched:** 2026-04-01
**Confidence:** HIGH (all findings from direct source code inspection)

---

## Standard Architecture

### System Overview

```
apps/level_editor/main.cpp   (orchestrator — owns all state, runs the frame loop)
  |
  +-- EditorSceneDocument     (authoritative scene graph — add/delete/duplicate/transform)
  +-- EditorCommandStack      (undo/redo — snapshot-based, 256-command ring buffer)
  +-- EditorPreviewWorld      (ECS preview — entt registry + MeshLibrary + bounds cache)
  +-- EditorUiState           (all panel toggle flags, selected tool, snapping state)
  +-- std::vector<uint64_t> selectedIds  (current selection — lives in main)
  +-- EditorSelectionPickerState         (overlap cycle state)
  +-- EditorPlacementState               (active drag-to-place operation)
  +-- EditorPendingCommand widgetCommand (inspector widget drag tracking)
  +-- EditorPendingCommand gizmoCommand  (gizmo drag tracking)
  |
  +- UI panels (render and return action results — no state ownership)
  |   +-- renderOutliner()        -> returns deleteRequests vector
  |   +-- renderInspector()       -> returns InspectorActionResult
  |   +-- renderAssetBrowser()    -> returns AssetBrowserActionResult
  |   +-- renderEnvironmentPanel()
  |
  +- Viewport pipeline (edit mode path only)
  |   +-- collectRenderObjects()          -> builds RenderObject list from PreviewWorld
  |   +-- appendHelperObjects()           -> colliders, light helpers, spawn
  |   +-- appendSelectionOverlays()       -> wireframe AABB overlays per selectedId
  |   +-- EditorViewportRenderer::render() -> SceneRenderPipeline -> OpenGL FBO
  |   +-- applyGizmoToSelectedObject()    -> ImGuizmo -> document.applyWorldTransform()
  |
  +- Action dispatch (end of frame, after all panels)
      +-- deletePressed / outlinerDeleteRequests
      +-- duplicatePressed
      +-- undoPressed / redoPressed
      +-- focusPressed
```

### Component Responsibilities

| Component | Responsibility | Key API |
|-----------|---------------|---------|
| `EditorSceneDocument` | Authoritative scene state — objects, environment, node IDs, parent/child hierarchy | `addMesh()`, `eraseObjects()`, `duplicateObject()`, `applyWorldTransform()`, `captureState()`, `restoreState()` |
| `EditorSceneDocumentState` | Plain serializable snapshot for undo/redo — contains `vector<EditorSceneObject>`, `EnvironmentDefinition`, `nextObjectId`, dirty flags | Value type copied by `captureState()` |
| `EditorCommandStack` | Snapshot-based undo/redo, 256-command ring, dirty-flag sync, serialize-then-diff deduplication | `pushDocumentStateCommand()`, `undo()`, `redo()` |
| `EditorPreviewWorld` | Mirrors document into an entt ECS registry for rendering; maintains per-object bounds and scene AABB | `rebuild()`, `syncMaterials()`, `syncLights()`, `findObjectBounds()` |
| `EditorSelectionPickerState` | Overlap-cycle popup state (hit list, current index, expiry timer) | Pure data; managed in main.cpp |
| `EditorUiState` | All panel visibility flags, active tool, snapping params, selected mesh/material/archetype IDs | `EditorTransformTool`, `snappingEnabled`, `selectedMeshId` |
| `EditorPlacementState` | Active drag-to-place operation (kind + ids) | `active()`, `clear()` |
| `EditorPendingCommand` | Tracks before-state for ongoing widget/gizmo drags; finalized on release | `beginPendingCommand()`, `finalizePendingCommand()` |
| `appendSelectionOverlays()` | Generates wireframe AABB RenderObjects for all selected IDs; uses `ignoreDepth=true` | Called each frame in edit path |

---

## How New Features Integrate

### Delete

**Status: Already implemented and fully wired.**

Both code paths are undo-tracked:

1. **Global hotkey** (`Delete` key, checked in main frame loop): sets `deletePressed = true`. Dispatched at end of frame — captures before-state, calls `document.eraseObjects(selectedIds)`, pushes command, clears selection, sets `previewDirty = true`.
2. **Outliner panel**: `renderOutliner()` returns a `deleteRequests` vector (populated by "Delete Selected" button, `Delete` key when outliner focused, or right-click context menu). Dispatched in main after panel call with the same snapshot pattern.
3. `eraseObjects()` automatically expands the ID list to include all children before erasing — child cascade is built in.

No new code is needed.

---

### Duplicate

**Status: Already implemented and fully wired.**

`Ctrl/Cmd+D` sets `duplicatePressed = true`. Dispatched at end of frame: iterates `selectedIds`, calls `document.duplicateObject(id)` for each, collects new IDs, replaces `selectedIds` with the duplicated IDs, pushes "Duplicate Selection" command, sets `previewDirty = true`.

`duplicateObject()` calls `addObject(kind, payload)` — copies the payload verbatim. Node IDs are cleared and regenerated in `addObject()` via `ensureObjectNodeId()`. Parent relationship in the payload is preserved in the copy. PlayerSpawn duplicates are blocked at the document level (returns 0).

No new code is needed.

---

### Add Mesh (via Asset Browser)

**Status: Already implemented.**

Flow: Asset browser emits `ImGui::SetDragDropPayload("EDITOR_PLACE", ...)` with an `EditorDragPayload`. The viewport receives the drop (or the user clicks with an active `EditorPlacementState`). `commitPlacement()` in `EditorViewportInteraction.cpp` calls `document.addMesh()`. A "Place Object" command is pushed.

The asset browser also calls `beginPlacement()` to set a placement mode that persists until the user clicks. Both flows go through `commitPlacement()` then `document.addMesh()`.

No new code is needed for the core flow. Any work here is discoverability polish — e.g., a dedicated "Add Mesh" button in the viewport toolbar or outliner header that opens a picker modal rather than requiring a drag.

---

### Selection Overlay Fix (visible through solid geometry in front)

**Status: Needs implementation.**

**The problem:** `appendSelectionOverlays()` in `EditorScenePreviewRenderer.cpp` generates one wireframe AABB `RenderObject` per selected ID with `ignoreDepth = true` and `wireframe = true`. `Renderer::drawScene()` disables `GL_DEPTH_TEST` when `ignoreDepth` is set. This causes the wireframe box to render on top of geometry that is physically in front of the selected object — the "ghost outline through walls" issue.

**The fix — two-pass overlay (recommended):**

Split each selection overlay into two `RenderObject` entries:
- Pass 1: `ignoreDepth = false`, `wireframe = true`, `lineWidth = 4.0f`, full tint — renders correctly clipped by geometry.
- Pass 2: `ignoreDepth = true`, `wireframe = true`, `lineWidth = 1.5f`, tint scaled down to ~25% — faint depth-independent hint, visible when object is behind walls.

This is the Unity selection approach. Pass 2 ensures you can always locate a selected but occluded object without the bright box overpowering what is in front.

**Simpler alternative:** Remove `ignoreDepth = true` from the overlay entirely. The wireframe clips at geometry boundaries. You lose visibility when the selected object is fully occluded but gain correct rendering.

**Architectural change required:**
- Modify `appendSelectionOverlays()` in `src/editor/render/EditorScenePreviewRenderer.cpp`
- Change `ignoreDepth = true` to `ignoreDepth = false` for the primary pass
- Optionally `push_back` a second entry per selected ID with reduced tint and `ignoreDepth = true`
- No changes to `RenderObject` struct, `Renderer`, or any other system

---

### Move/Translate with Gizmos

**Status: Already implemented.**

`applyGizmoToSelectedObject()` in `EditorViewportInteraction.cpp` handles all three tools (Translate/Rotate/Scale via `EditorTransformTool`). It delegates to `manipulateEditorGizmo()` → ImGuizmo, then feeds the result back into `document.applyWorldTransform()`. Pending command tracking uses `gizmoCommand` (`EditorPendingCommand`) in main.

The gizmo currently requires exactly one object selected (`selectedIds.size() != 1` exits early). Multi-object gizmo is not implemented.

No new code is needed for single-object transform.

---

## Data Flow

### Command Lifecycle (any mutating operation)

```
User action (key, button, outliner, viewport)
    |
beforeState = document.captureState()   [snapshot EditorSceneDocumentState]
    |
document.eraseObjects() / addMesh() / duplicateObject() / applyWorldTransform() etc.
    |
commandStack.pushDocumentStateCommand(label, beforeState, document.captureState(), document)
    |  [internally diffs serialized scene strings -- no-op if nothing changed]
previewDirty = true
    |
next frame: previewWorld.rebuild(document, content)
```

### Selection Flow

```
Left click in viewport
    |
buildEditorRay() -> pickEditorObjects(viewportSelectionHandles, ray)
    |  [returns sorted hit list by distance]
refreshSelectionPicker()  [multi-hit overlap cycle -- click again to cycle]
    |
applySelectionHit() -> toggleSelection(selectedIds, id, additive)
    |
selectedIds updated -> next frame: appendSelectionOverlays() uses updated list
```

### Preview Sync Model

Two tiers of update cost:

```
document mutation -> markSceneDirty() -> ++sceneRevision_

main.cpp per-frame check:
  if (previewDirty)              -> previewWorld.rebuild()          [full ECS rebuild -- expensive]
  elif (sceneRevision mismatch)  -> previewWorld.syncMaterials()
                                    + previewWorld.syncLights()     [cheap -- no ECS rebuild]
  elif (envRevision mismatch)    -> runtime session environment sync only
```

`previewDirty = true` is the expensive path. It is set when objects are added, removed, or a mesh ID changes. Transform-only changes (gizmo drag) go through the cheap `syncMaterials`/`syncLights` path via revision counters automatically.

---

## Recommended File Layout

No new directories needed. All changes stay in existing files:

```
src/editor/
+-- scene/
|   +-- EditorSceneDocument.h/.cpp    [NO CHANGES -- add/delete/duplicate already complete]
|   +-- EditorSelectionSystem.h/.cpp  [NO CHANGES]
+-- render/
|   +-- EditorScenePreviewRenderer.cpp  [MODIFY: appendSelectionOverlays() depth fix]
+-- ui/
|   +-- EditorOutlinerPanel.cpp       [NO CHANGES -- delete path complete]
|   +-- EditorPanels.h                [NO CHANGES]
+-- viewport/
    +-- EditorViewportInteraction.cpp [NO CHANGES -- gizmo/placement complete]

apps/level_editor/
+-- main.cpp   [NO CHANGES -- delete/duplicate/undo/redo all dispatched correctly]
```

---

## Architectural Patterns

### Pattern 1: Snapshot Command (all mutations)

**What:** Capture full `EditorSceneDocumentState` before and after each mutation. Push to `EditorCommandStack`. Undo/redo restores snapshots.

**When to use:** Every operation that mutates `EditorSceneDocument`.

**Trade-offs:** Full snapshot per command. At 200 objects, roughly 50-200 KB per entry. 256-command limit caps peak memory at ~50 MB. Acceptable for an editor tool.

**Correct pattern:**
```cpp
const EditorSceneDocumentState beforeState = document.captureState();
document.eraseObjects(selectedIds);
commandStack.pushDocumentStateCommand(
    "Delete Selection", beforeState, document.captureState(), document);
previewDirty = true;
```

### Pattern 2: Deferred Action via Flag (avoid re-entrancy mid-render)

**What:** UI panels set boolean flags or return result structs. Actions are dispatched after all panels render, at the bottom of the frame lambda.

**When to use:** All destructive or selection-modifying operations triggered from panels.

**Why:** ImGui panels render mid-frame. Mutating `document` or `selectedIds` during rendering invalidates panel state for panels that render later in the same frame.

### Pattern 3: PendingCommand for Drag Operations

**What:** For operations spanning multiple frames (gizmo drag, inspector DragFloat), `EditorPendingCommand` holds the `beforeState` from drag start. Finalized with `finalizePendingCommand()` on drag release, producing a single undo entry.

**Example:**
```cpp
// Frame N: gizmo just became hot
beginPendingCommand(gizmoCommand, beforeState, "Transform Object");

// Frame N+k: gizmo released
finalizePendingCommand(gizmoCommand, commandStack, document);
```

### Pattern 4: previewDirty vs. sceneRevision (two-tier sync)

**What:** `previewDirty = true` forces full ECS rebuild. The light path activates automatically when `sceneRevision` ticks without `previewDirty`.

**When to set `previewDirty = true`:** Object added, removed, mesh ID changed, scene loaded.
**Let revision sync handle:** Material edits, light tweaks, gizmo-only transforms.

---

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| main.cpp <-> EditorSceneDocument | Direct method calls | main owns the instance; all mutations go through document methods |
| main.cpp <-> EditorCommandStack | `pushDocumentStateCommand()` + `undo()`/`redo()` | Stack takes ownership of before/after state snapshots |
| main.cpp <-> UI panels | Function call in / result struct out | Panels do not mutate document directly |
| EditorOutlinerPanel <-> main | Returns `vector<uint64_t> deleteRequests` | Deletion deferred to main for dispatch consistency |
| EditorPreviewWorld <-> EditorSceneDocument | `rebuild(document, content)` called from main | EditorPreviewWorld reads document; document does not know about PreviewWorld |
| appendSelectionOverlays <-> RenderObject | Writes `wireframe=true`, `ignoreDepth=true` to RenderObject entries | This is the single point to change for the depth fix |

### New vs Modified for Each Feature

| Feature | Status | Files to Change |
|---------|--------|-----------------|
| Delete (hotkey + outliner) | Complete | None |
| Duplicate (Ctrl+D) | Complete | None |
| Add mesh via asset browser / drag | Complete | None |
| Move with gizmo (single selection) | Complete | None |
| Selection overlay depth fix | Needs implementation | `src/editor/render/EditorScenePreviewRenderer.cpp` only |

---

## Scaling Considerations

Single-user desktop editor. No distributed scaling concerns. Only relevant performance boundary:

| Concern | Current Approach | Threshold |
|---------|-----------------|-----------|
| Undo command memory | 256-entry ring, snapshot per command | ~50 MB at 200 objects; acceptable |
| PreviewWorld rebuild | Full ECS teardown + rebuild | Visible latency at >500 objects; use syncMaterials path for transform-only |
| Selection overlay CPU | One or two RenderObjects per selected ID per frame | Negligible for typical selections (<20 objects) |

---

## Anti-Patterns

### Anti-Pattern 1: Mutating Document During Panel Render

**What people do:** Call `document.eraseObjects()` or `document.addMesh()` directly inside an ImGui panel function.

**Why it's wrong:** ImGui renders panels sequentially in a single frame. Mutating `selectedIds` or `document` mid-frame means later panels get stale data or hit iterator invalidation.

**Do this instead:** Return an action result struct or set a deferred flag. Dispatch mutations after all panels finish (the pattern throughout main.cpp).

### Anti-Pattern 2: Setting previewDirty on Every Frame During Gizmo Drag

**What people do:** Call `previewDirty = true` every frame inside gizmo drag handling.

**Why it's wrong:** `previewWorld.rebuild()` tears down and recreates the entire ECS. At 60 fps during a drag, this causes visible stutter.

**Do this instead:** Gizmo drag calls `document.applyWorldTransform()` which increments `sceneRevision_`. main.cpp's cheap revision check handles it via `syncMaterials()` + `syncLights()` automatically.

### Anti-Pattern 3: Capturing Before-State After Mutation

**What people do:** Capture state after the mutation, then pass it as both before and after to `pushDocumentStateCommand()`.

**Why it's wrong:** `pushDocumentStateCommand()` diffs serialized scene strings. Before == after means no command is pushed — the undo entry is silently dropped.

**Do this instead:** Always `const EditorSceneDocumentState beforeState = document.captureState()` before any mutation.

### Anti-Pattern 4: Using ignoreDepth for Primary Selection Overlay

**What people do (current implementation):** Set `ignoreDepth = true` on the selection wireframe so it always shows on top.

**Why it's wrong:** The overlay renders on top of geometry in front of the selected object, making it appear to "bleed through walls." Distracting when selecting objects behind other geometry.

**Do this instead:** Two-pass overlay — primary pass with depth testing enabled (correct clipping), optional secondary pass with `ignoreDepth = true` at very low opacity for occluded hint.

---

## Build Order for the Milestone

All core operations (delete, duplicate, move, add mesh) are already shipped. The only remaining implementation work is:

```
Step 1: Selection overlay depth fix
        File: src/editor/render/EditorScenePreviewRenderer.cpp
        Change: appendSelectionOverlays() -- remove ignoreDepth=true from primary pass,
                optionally add a secondary low-opacity pass for occluded hint
        No blockers, no dependencies, immediately visible result

Step 2 (if needed): Multi-selection gizmo
        File: src/editor/viewport/EditorViewportInteraction.cpp
        Change: applyGizmoToSelectedObject() -- remove selectedIds.size() != 1 guard,
                compute centroid/average transform for multi-selection
        Depends on: Step 1 complete (so overlays show correctly during multi-selection)

Step 3 (if needed): Add Mesh UX polish (modal picker vs. drag-only)
        File: src/editor/ui/EditorOutlinerPanel.cpp or apps/level_editor/main.cpp
        Change: Add "Add Mesh" button to outliner/viewport toolbar that opens a searchable
                picker popup calling beginPlacement()
        Depends on: Nothing
```

---

## Sources

All findings from direct source code inspection (no training-data assumptions):

- `apps/level_editor/main.cpp` — orchestrator, frame loop, action dispatch
- `src/editor/scene/EditorSceneDocument.h/.cpp` — scene graph API
- `src/editor/core/EditorCommand.h/.cpp` — undo/redo stack
- `src/editor/viewport/EditorViewportInteraction.h/.cpp` — selection, gizmo, placement
- `src/editor/render/EditorScenePreviewRenderer.h/.cpp` — selection overlays, helper objects
- `src/editor/ui/EditorOutlinerPanel.cpp` — outliner delete path
- `src/editor/ui/LevelEditorUi.h` — shared UI structs, pending command helpers
- `src/editor/core/LevelEditorCore.h` — layout/scene load helpers
- `src/engine/rendering/geometry/Renderer.h/.cpp` — RenderObject struct, wireframe/depth rendering

Confidence: HIGH — all claims verified against current source.

---
*Architecture research for: custom C++ game engine level editor UX features*
*Researched: 2026-04-01*
