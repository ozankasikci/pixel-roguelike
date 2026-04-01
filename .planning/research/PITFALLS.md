# Pitfalls Research

**Domain:** ImGui-based level editor — adding object manipulation (delete, duplicate, add mesh, selection)
**Researched:** 2026-04-01
**Confidence:** HIGH (based on direct code inspection of the existing editor + verified ImGui/ImGuizmo source issues + community post-mortems)

---

## Critical Pitfalls

### Pitfall 1: Delete Key Fires While Editing Text in the Inspector

**What goes wrong:**
The user renames a node in the inspector's InputText field. They press Delete to remove a character. The keystroke simultaneously triggers the scene-object deletion shortcut, deleting the selected scene object. The user loses their mesh without intending to.

This is a confirmed Dear ImGui bug (issue #8048, fixed in commit 661bba0). Before the fix, `InputTextEx` did not call `SetKeyOwner(ImGuiKey_Delete, id)` when the widget was active, so the key escaped to parent window shortcuts.

**Why it happens:**
The existing outliner checks `!ImGui::GetIO().WantTextInput` before handling `ImGuiKey_Delete`, which is correct. However, `WantTextInput` is only true when ImGui wants *text* input — it does not prevent all key routing. The viewport Delete key handler (in `main.cpp`, the `deletePressed` logic) also needs to check `ImGui::IsWindowFocused` on the viewport window, not just `WantTextInput`. If the viewport and inspector are docked and the inspector InputText is active, the focus check may pass incorrectly.

Additionally, any shortcut added to the viewport interaction code that uses raw `ImGui::IsKeyPressed(ImGuiKey_Delete)` without a `!ImGui::GetIO().WantCaptureKeyboard` guard is vulnerable in ImGui versions before the fix.

**How to avoid:**
- Gate all viewport keyboard shortcuts behind `!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()`. The existing outliner Delete path already uses this pattern correctly; the viewport Delete path (`deletePressed` in main.cpp) must follow the same guard.
- For Ctrl+D duplicate: use `ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteFocused)` rather than raw `IsKeyPressed`, which respects ImGui's key ownership routing.
- Pin to Dear ImGui v1.90+ where InputText key ownership is correct.

**Warning signs:**
- Typing in the inspector's node-name field and pressing Delete accidentally removes the object
- Any shortcut path that calls `ImGui::IsKeyPressed(key)` without checking focus or `WantTextInput`

**Phase to address:**
Phase implementing Delete key shortcut (delete scene objects). Must also audit Ctrl+D duplicate when added.

---

### Pitfall 2: Delete Does Not Push an Undo Command

**What goes wrong:**
The viewport-key Delete path and the outliner Delete path both correctly call `document.eraseObjects()` and then `commandStack.pushDocumentStateCommand()`. But when a **new** delete entry point is added (e.g., a viewport right-click context menu, or a Del key shortcut in the asset browser), the developer adds only the `eraseObjects()` call and forgets the `captureState()` before + `pushDocumentStateCommand()` after pattern. Deletions become non-undoable.

The existing code uses a capture-before / push-after pattern consistently. Every new code path that mutates `EditorSceneDocument` must independently re-implement this pattern because there is no automatic mutation hook.

**Why it happens:**
The pattern is not enforced by the type system. `eraseObjects()` returns void and does not capture state. The developer's mental model is "erasing removes from the document" without the separate undo step being obvious. Copy-paste errors where only the erase call is copied from an example.

**How to avoid:**
- Create a helper in `LevelEditorUi.h` (the `EditorPendingCommand` / `trackLastItemCommand` pattern already exists for continuous edits; add a `commitImmediateCommand(label, before, after, stack, doc)` analogue that is the required path for all discrete mutations).
- Code review checklist: every call to `document.eraseObjects()`, `document.duplicateObject()`, or any `addX()` method must be accompanied by a surrounding capture-push pair.
- Write a test: delete a mesh, undo, verify the mesh is back with the same ID.

**Warning signs:**
- A new code path calls `eraseObjects()` but there is no `captureState()` call within 5 lines above it
- Deletion from a right-click context menu cannot be undone (Ctrl+Z does nothing)

**Phase to address:**
Phase implementing Delete. Also applies to any subsequent Duplicate or Add Mesh phases.

---

### Pitfall 3: Selection Vector Contains Stale IDs After Delete + Undo

**What goes wrong:**
User selects three objects. Deletes them. The selection vector `selectedIds` is cleared (correctly). User presses Ctrl+Z. The undo restores the document state, but `selectedIds` (which lives outside the document) still holds `{}`  — the user sees no selection restored. If the user had instead pressed Ctrl+Z after a subsequent action, `selectedIds` may still reference IDs that no longer exist in the document. The inspector then tries to render controls for `document.findObject(id)`, which returns `nullptr`, and either crashes or silently renders nothing.

A subtler case: user selects object A, duplicates it (getting A'), presses undo, selects something else. Now `selectedIds` may still contain A'. If the user then tries to operate on A', `findObject(A')` returns nullptr because A' no longer exists post-undo.

**Why it happens:**
`selectedIds` is a `std::vector<std::uint64_t>` that lives in `main.cpp` alongside the document. Undo/redo restores `EditorSceneDocumentState`, which does not include selected IDs. The disconnect between document state and selection state is architectural — by design, selection is transient UI state. But this means the caller is responsible for pruning after every undo/redo.

The existing `pruneSelection()` helper exists precisely for this reason and is called after outliner deletes. It must also be called after every undo/redo operation.

**How to avoid:**
- Call `pruneSelection(document, selectedIds)` immediately after every call to `commandStack.undo()` and `commandStack.redo()`.
- Verify: after undo, `selectedIds` only contains IDs that exist in the document.
- Any code path that reads `selectedIds` before calling `document.findObject()` must null-check the result even if the id supposedly came from selection.

**Warning signs:**
- Inspector panel crashes or goes blank after Ctrl+Z
- `document.findObject(id)` returns nullptr in the inspector render path
- `pruneSelection` is not called in the undo/redo handler

**Phase to address:**
Phase implementing Undo/Redo wiring for Delete. Must be verified for all future phases that add operations.

---

### Pitfall 4: Duplicate Produces Objects With Colliding Node IDs

**What goes wrong:**
The existing `duplicateObject()` calls `addObject(object->kind, object->payload)`, which copies the entire `payload` verbatim. For `LevelMeshPlacement` and other placement types that contain a `nodeId` string field, the duplicate gets the same `nodeId` as the original. When the scene is saved and reloaded, `EditorSceneSerializer` processes two objects with the same `nodeId`. The serializer overwrites one with the other, silently losing a mesh. The user only notices on the next load.

**Why it happens:**
The `payload` copy is intentional for the transform/material data. The `nodeId` field looks like data and gets copied along with everything else. The issue is invisible at runtime — it only manifests on the serialization round-trip.

**How to avoid:**
- In `duplicateObject()`, after copying the payload, clear/regenerate any `nodeId` fields. Check `LevelMeshPlacement`, `LevelLightPlacement`, `LevelArchetypePlacement`, and any other payload type that has a `nodeId` member.
- The existing `ensureObjectNodeId()` method can be used to assign fresh node IDs after duplication.
- Write a round-trip test: duplicate an object, save, reload, verify two distinct objects exist with distinct node IDs.

**Warning signs:**
- Scene has N duplicated meshes at edit time but only N/2 on reload
- `ensureObjectNodeId()` is never called in the duplication path
- Two `EditorSceneObject` instances have the same value in their `nodeId` payload field

**Phase to address:**
Phase implementing Duplicate.

---

### Pitfall 5: Duplicate Places Object Exactly on Top of the Original

**What goes wrong:**
Ctrl+D duplicates the selected object with position (0,0,0) offset — meaning the new object occupies the exact same world position as the original. The user cannot tell anything happened. They click somewhere else, come back, and discover N stacked meshes instead of N separate objects.

**Why it happens:**
`duplicateObject()` copies the payload including the `position` field. No offset is applied. The developer test is "does a second object appear in the outliner" — which passes — but doesn't test whether the objects are visually distinguishable.

**How to avoid:**
- Apply a small world-space offset on duplication. The standard Unity/Unreal convention is `(0.5, 0, 0)` in world units, or — preferably — place the duplicate slightly in front of the current editor camera.
- Alternatively, use the existing `computePlacementPoint()` machinery to place the duplicate at the camera-forward ray intersection, which is the most ergonomic behavior.
- Select the duplicate immediately after creation so the user sees it highlighted and can move it directly.

**Warning signs:**
- Outliner shows two entries with identical labels at the same position
- User discovers stacked meshes only when moving one
- After Ctrl+D, the selection does not transfer to the new duplicate

**Phase to address:**
Phase implementing Duplicate.

---

### Pitfall 6: State Snapshot Comparison Marks Scene Dirty on Every Frame

**What goes wrong:**
The `EditorCommandStack::syncDirtyFlags()` method serializes the entire scene to a string on every call to compare against the saved signature. If this method is called during the ImGui render loop (not just after mutations), the serialization overhead is constant and grows with scene size. For scenes with hundreds of meshes, this produces a multi-millisecond stall every frame, making the editor feel unresponsive.

A secondary issue: if `syncDirtyFlags()` is called at the wrong time (e.g., after a transient environment preview state is applied but before it is reverted), the serialized string temporarily differs from the saved signature and the scene appears dirty — the title bar shows an asterisk even though no real changes were made.

**Why it happens:**
The `syncDirtyFlags()` call appears at the end of every `undo()`, `redo()`, and `pushDocumentStateCommand()` call. This is correct. But the developer adds an additional call in the main render loop to "keep things in sync," not realizing the cost.

**How to avoid:**
- Never call `syncDirtyFlags()` outside of the explicit mutation points (undo, redo, push). The existing pattern is correct.
- Profile with a large scene (200+ objects) to verify dirty-check overhead stays below 1ms.
- Environment-only changes should not pollute the scene dirty flag. The existing separation of `sceneDirty_` and `environmentDirty_` must be preserved when adding new mutation paths.

**Warning signs:**
- Editor framerate drops noticeably when many objects are in the scene
- Scene appears dirty (asterisk in title) immediately after loading without any edits
- `syncDirtyFlags()` or `captureState()` appears in the per-frame render path

**Phase to address:**
Phase implementing any new mutation path (Delete, Duplicate, Add Mesh). Each one introduces a potential extra `captureState()` per-frame if wired incorrectly.

---

### Pitfall 7: Add Mesh Places Object at World Origin, Invisible to the User

**What goes wrong:**
"Add Mesh" spawns the object at `(0, 0, 0)`. If the editor camera is looking at a scene 50 units away from the origin, the new mesh is behind the camera or clipped by the near plane and invisible. The user presses Add Mesh repeatedly wondering if it worked.

**Why it happens:**
The placement origin (0,0,0) is the obvious default. The developer test passes — "object appears in outliner." The test does not verify the object is visible in the viewport.

**How to avoid:**
- Use `computePlacementPoint()` (which already exists in `EditorViewportInteraction`) to raycast from the camera center and place the object at the hit surface, or at a fixed distance in front of the camera if no surface is hit (3–5 units ahead, matching the scene scale).
- This is the same code path as drag-and-drop mesh placement; reuse it.
- If raycasting is not available at this point, place the object at `camera.position + camera.forward * 3.0f`.
- After adding, select the new object and issue `ui.frameSelectionRequested = true` so the viewport frames to show it.

**Warning signs:**
- Added objects do not appear visible in the viewport without manually navigating to origin
- No raycast or camera-forward calculation in the "add" code path
- `frameSelectionRequested` is not set after adding

**Phase to address:**
Phase implementing Add Mesh.

---

### Pitfall 8: Selection Overlay Renders Through Occluding Geometry

**What goes wrong:**
The selected-object highlight (wireframe overlay or tint) renders on top of all other geometry, even when the selected object is fully behind a wall. The user selects the wrong object, looks at a wall, and sees the selection highlight bleeding through. This is the issue specifically named as a target fix in the v1.1 milestone ("remove distracting selection overlay when another mesh is underneath").

**Why it happens:**
Overlays that draw in a separate post-pass without reading the depth buffer will always render on top. Standard stencil-based selection highlights work this way by design — the stencil mask is set during the scene pass, then the outline is drawn ignoring depth.

**How to avoid:**
- The selection highlight must respect the depth buffer. Two options:
  1. Draw the highlight mesh normally with depth testing enabled, using a slight scale-up or fragment-shader outline. Hidden objects get their highlights occluded correctly.
  2. Render the highlight to a separate FBO, then composite it with depth awareness by sampling the scene depth and discarding highlight fragments where `highlightDepth > sceneDepth`.
- The simpler approach for this codebase: render a second "outline pass" for selected objects after the scene pass, with depth test mode set to `GL_EQUAL` (draw only where the object is actually visible) or `GL_LEQUAL` with a small polygon offset.
- Keep the selection rendering as a configurable option that can be toggled — different workflows benefit from see-through vs. depth-correct selection.

**Warning signs:**
- Selected object's highlight is visible through walls when walking away
- Selection highlight does not change visual intensity as the object becomes more occluded
- The overlay draw call uses `glDisable(GL_DEPTH_TEST)` unconditionally

**Phase to address:**
Phase implementing selection overlay improvements (directly named in the milestone goals).

---

### Pitfall 9: Undo Stack Grows Unbounded During Continuous Gizmo Drags

**What goes wrong:**
Every mouse-move event during a gizmo drag calls `applyGizmoToSelectedObject()`, which calls `finalizePendingCommand()`, which pushes a new state to the undo stack. A 2-second drag produces 120 undo steps at 60fps, each storing a full document snapshot. Pressing Ctrl+Z requires 120 presses to undo one logical drag operation. Memory usage spikes for large scenes.

**Why it happens:**
The gizmo uses an `EditorPendingCommand` (capturing state before the drag begins) and only finalizes when `ImGuizmo::IsUsing()` transitions from true to false. This is the correct pattern — but it is fragile. If a new continuous-input path is added (e.g., scrollwheel to adjust a value) without using `EditorPendingCommand`, it falls back to per-frame push.

**How to avoid:**
- All continuous interactions (gizmo drag, slider drag, scroll-to-resize) must use the `beginPendingCommand()` / `finalizePendingCommand()` pair, never `pushDocumentStateCommand()` per frame.
- The existing `trackLastItemCommand()` helper handles this for ImGui slider/drag widgets. Every new continuous widget must use it.
- Verify: drag a mesh with the gizmo 10 times, press Ctrl+Z 10 times — exactly 10 undo steps should be consumed, not 600+.

**Warning signs:**
- Ctrl+Z requires multiple presses to undo a single drag
- Memory usage grows linearly during a scene-editing session
- Any call to `pushDocumentStateCommand()` inside a loop or per-frame code path

**Phase to address:**
Phase implementing Move/Translate with gizmo support. Also applies to any future resize/scale operations.

---

### Pitfall 10: Keyboard Shortcut Fires in Both Viewport and Outliner Simultaneously

**What goes wrong:**
The user has the outliner focused and presses Ctrl+D intending to duplicate a selected object. The viewport's Ctrl+D handler also fires because the shortcut is checked against `ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)` on the viewport, which returns true even when the outliner is focused (both are children of the root dockspace). The object is duplicated twice.

**Why it happens:**
ImGui docking makes window focus ambiguous. `ImGuiFocusedFlags_RootAndChildWindows` is designed to catch the entire docked window hierarchy, not a specific panel. When two panels both use this flag for the same shortcut, both fire.

**How to avoid:**
- Implement shortcuts as **global** handlers in `main.cpp` (the top-level per-frame logic), not inside individual panel render functions. A single authoritative check fires once per frame.
- Use `ImGui::Shortcut(key, ImGuiInputFlags_RouteGlobal)` for editor-wide shortcuts (Delete, Ctrl+Z, Ctrl+Y, Ctrl+D). The per-panel keyboard handlers (like the existing outliner Delete check) should be removed and consolidated into the global handler.
- The existing `deletePressed` boolean in `main.cpp` follows this pattern for the viewport. Duplicate and Add should follow the same pattern.

**Warning signs:**
- Same shortcut checked in both a panel render function AND in main.cpp
- `ImGui::IsKeyPressed()` appears inside `renderOutliner()` or `renderInspector()` for shortcuts that should be global
- Duplicate fires twice when tested with outliner + viewport both visible

**Phase to address:**
Phase implementing Ctrl+D duplicate shortcut. Audit all keyboard shortcut handling at that time.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Per-frame `captureState()` to keep undo fresh | Always have the "before" state | Serializes entire scene every frame; O(n) on scene size | Never — use `beginPendingCommand` / `finalizePendingCommand` |
| Skip undo push for "minor" operations (e.g., position nudge) | Simpler code | Users lose work; destroys trust in undo reliability | Never — all mutations go through the stack |
| Place duplicates at origin (0,0,0) | Zero code for placement | Objects pile up invisibly; users think the command failed | Never in shipped editor — always offset from camera |
| Global `ImGui::IsKeyPressed()` without focus check | Simple shortcut implementation | Fires in text fields, causes data loss | Never for destructive actions (Delete, Duplicate) |
| Use `ImGuiTreeNodeFlags_Selected` on all ancestors when a child is selected | Makes tree look sensibly highlighted | State bloat, inconsistent with parent/child selection semantics | Fine as visual hint if selection vector is authoritative |

---

## Integration Gotchas

Common mistakes when connecting to ImGui/ImGuizmo in this editor.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| ImGuizmo | Calling `ImGuizmo::SetRect` with the wrong origin when docked | `ImGuizmo::SetRect` must use the viewport panel's screen-space origin (`viewport.origin.x`, `viewport.origin.y`) not `(0, 0)`. Use `EditorViewportState::origin`. |
| ImGuizmo | Gizmo appears but does not react to mouse | `ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList())` must be called inside the viewport's `ImGui::Begin` scope, not in the global scope. |
| ImGui `InputText` + Delete key | Delete removes character AND scene object | Guard scene-delete shortcuts with `!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()` |
| ImGui drag-drop from asset browser | Payload data is a raw `char[]` but consumer casts to wrong type | The `EditorDragPayload` struct is 132 bytes. Verify `payload->DataSize == sizeof(EditorDragPayload)` before casting. |
| Scene dirty flag | `markSceneDirty()` called during undo/redo restore | `restoreState()` internally increments the revision counter; the caller must not additionally call `markSceneDirty()` after restore or the dirty comparison diverges from the saved signature. |
| Selection after undo | Selection vector references deleted IDs | Always call `pruneSelection(document, selectedIds)` immediately after `commandStack.undo()` and `commandStack.redo()`. |

---

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Full document serialize on every `syncDirtyFlags()` call | Frame stutter when selecting objects with large scenes | Only serialize inside undo/redo/push calls, never in the frame loop | ~100+ objects depending on payload size |
| `childObjectIds()` called O(n²) in outliner render | Outliner redraws get slower as hierarchy deepens | Cache child lists if hierarchy doesn't change between frames, or use depth-first iteration | ~50+ objects with deep parenting |
| `buildEditorSelectionHandles()` rebuilds AABB for all objects every mouse move | Raycasting in the viewport stalls at frame start | Only rebuild when `document.sceneRevision()` changes | ~200+ objects |
| Storing `EditorSceneDocumentState` (full objects vector) in every undo step | Memory grows with each edit session | The 256-command cap (`kMaxCommands`) prevents unbounded growth; do not increase this limit without measuring | Beyond 256 commands with large scenes |

---

## UX Pitfalls

Common user experience mistakes specific to this editor domain.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Delete with no confirmation for complex objects (archetypes with children) | User accidentally deletes a subtree, notices 10 edits later | For multi-object or parent-with-children deletes, flash a brief status message ("Deleted 5 objects — Ctrl+Z to undo") rather than a blocking dialog |
| Duplicate does not transfer selection to the new object | User immediately presses G to move but moves the original | Always select the new duplicate immediately after creation |
| Add Mesh picker requires knowing the mesh string ID | Steep learning curve; prone to typos | The asset browser drag-and-drop already provides discoverability; "Add" should open the same picker, not require free-text entry |
| Ctrl+Z works but Ctrl+Y (redo) has no shortcut | Users trained on Ctrl+Y from other tools are confused | Wire both Undo and Redo in the same global shortcut handling block |
| Selection highlight visible through walls | Distracting; user cannot tell what is selected vs. what is behind | Depth-correct highlight (see Pitfall 8) |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Delete:** Object removed from outliner AND from document AND undo command pushed AND selection pruned AND `previewDirty` set to rebuild preview world.
- [ ] **Duplicate:** New object in outliner AND new object has distinct node ID AND new object is selected AND offset applied AND undo command pushed AND `previewDirty` set.
- [ ] **Add Mesh:** Object in outliner AND placed near camera (not at origin) AND selected after add AND undo command pushed AND `previewDirty` set.
- [ ] **Selection overlay:** Only visible for objects visible to the camera (not bleeding through walls) — verify by placing a mesh behind a wall and selecting it.
- [ ] **Keyboard shortcuts:** All shortcuts guarded by `!WantTextInput` — verify by renaming a node in the inspector and pressing the shortcut keys.
- [ ] **Undo of delete:** After deleting then undoing, the object is restored with the same ID, same position, same material, and is re-selectable.
- [ ] **Undo of duplicate:** After duplicating then undoing, exactly one copy exists (not zero, not two).

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Duplicate produces colliding node IDs (found after shipping) | MEDIUM | Add a scene migration pass in `EditorSceneSerializer::load()` that detects duplicate node IDs and regenerates them with a deterministic suffix. No data loss — just a one-time fix on load. |
| Delete fires in InputText (found in QA) | LOW | Add `!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()` guard to all shortcut checks. 10-line fix. |
| Undo stack bloated (N×60 steps per drag) | LOW | Verify that all gizmo and slider paths use `EditorPendingCommand`; replace any `pushDocumentStateCommand` in per-frame paths with `beginPendingCommand` / `finalizePendingCommand`. |
| Selection overlay bleeds through walls | MEDIUM | Requires shader change to the selection highlight pass. Add depth testing to the overlay draw call (`GL_LEQUAL` depth test). 1–2 day fix for a developer familiar with the render pipeline. |
| Scene appears dirty on load without edits | LOW | Verify `markSceneDirty()` is not called in the `restoreState()` path or in any initialization code path that runs after `commandStack.reset()`. |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Delete key fires in InputText | Delete phase | Type in inspector, press Delete — inspector text changes, no object deleted |
| Delete missing undo push | Delete phase | Delete object, Ctrl+Z, object restored with same ID |
| Stale selection IDs after undo | Delete phase | Delete, undo, verify `selectedIds` is empty or valid |
| Duplicate node ID collision | Duplicate phase | Duplicate, save, reload, verify two distinct objects |
| Duplicate lands on top of original | Duplicate phase | Duplicate, verify visually distinct placement in viewport |
| Add Mesh at origin | Add Mesh phase | Add mesh with camera at (50,50,50) — mesh appears near camera |
| Selection overlay through walls | Selection overlay phase | Select object behind wall — no highlight bleeds through |
| Undo stack bloat from continuous drags | Move/Translate phase | Drag gizmo 10 times, verify exactly 10 undo steps consumed |
| Duplicate shortcut fires twice | Ctrl+D shortcut phase | Duplicate with outliner focused — exactly one new object in scene |
| Scene dirty on load | Any mutation phase | Load scene, verify no dirty indicator without edits |

---

## Sources

- Direct code inspection: `/apps/level_editor/main.cpp`, `/src/editor/ui/EditorOutlinerPanel.cpp`, `/src/editor/scene/EditorSceneDocument.cpp`, `/src/editor/core/EditorCommand.cpp`, `/src/editor/viewport/EditorViewportInteraction.cpp`
- Dear ImGui issue #8048 — InputText does not take Delete key ownership: https://github.com/ocornut/imgui/issues/8048
- Dear ImGui issue #6621 — Global keyboard shortcut not triggered by InputText: https://github.com/ocornut/imgui/issues/6621
- ImGuizmo issue #292 — Drawing ImGuizmo elements behind/in front of geometry: https://github.com/CedricGuillemet/ImGuizmo/issues/292
- GameDev.net — Custom editor undo/redo system: https://www.gamedev.net/forums/topic/678496-custom-editor-undoredo-system/5290700/
- Wolfire Games blog — How We Implement Undo: http://blog.wolfire.com/2009/02/how-we-implement-undo/
- Wayline — Level Design Undo/Redo Mastery: https://www.wayline.io/blog/level-design-undo-redo-mastery

---
*Pitfalls research for: ImGui-based level editor — v1.1 Editor UX milestone*
*Researched: 2026-04-01*
