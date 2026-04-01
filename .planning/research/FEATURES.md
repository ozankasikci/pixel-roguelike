# Feature Research

**Domain:** Level editor UX — professional scene object manipulation (Unity/Unreal parity)
**Researched:** 2026-04-01
**Confidence:** HIGH (cross-verified against Unity Manual, Unreal Engine docs, and existing codebase)
**Milestone:** v1.1 Editor UX

---

## Context: What Already Exists

The editor has substantial infrastructure. Before calling something a new feature, confirm against what the codebase already ships:

| Already Built | Evidence |
|---------------|----------|
| Ray-cast click selection (single object) | `EditorSelectionSystem.cpp`, `pickEditorObject()` |
| Multi-select with Shift (range in outliner) | `EditorOutlinerPanel.cpp` lines 107–124 |
| Multi-select with Ctrl/Cmd click (toggle) | `EditorOutlinerPanel.cpp` line 51–52 |
| Selection cycling popup (multiple overlapping hits) | `EditorSelectionPickerState`, `refreshSelectionPicker()`, `renderSelectionPicker()` |
| Translate/Rotate/Scale gizmos via ImGuizmo | `applyGizmoToSelectedObject()` in `EditorViewportInteraction.h` |
| Grid snapping (configurable move/rotate/scale snap) | `EditorUiState::snappingEnabled`, `moveSnap`, `rotateSnap`, `scaleSnap` |
| Delete key in outliner panel (when outliner focused) | `EditorOutlinerPanel.cpp` line 81, `ImGuiKey_Delete` |
| Delete button in outliner UI | `EditorOutlinerPanel.cpp` line 71 |
| `duplicateObject()` method on `EditorSceneDocument` | `EditorSceneDocument.cpp` line 602 |
| Drag-to-viewport mesh placement from asset browser | `commitPlacement()`, `EditorDragPayload`, `emitPlacementDragSource()` |
| Placement ghost preview while dragging | `appendPlacementGhost()` |
| Undo/Redo command stack (256-step) | `EditorCommandStack`, `EditorDocumentStateCommand` |
| Object parenting / hierarchy (parent-child relationships) | `setParent()`, `clearParent()`, `childObjectIds()` in `EditorSceneDocument.h` |
| Drag-to-reorder in outliner | `moveObjectBefore()`, `moveObjectAfter()`, `moveObjectToRootEnd()` |
| Collider, light, archetype, spawn placement objects | `EditorSceneObjectKind` enum (6 kinds) |
| Scene save/load | `EditorSceneDocument::save()`, `loadFromSceneFile()` |
| Frame selection camera focus request | `EditorUiState::frameSelectionRequested` flag |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that define a professional-quality level editor. Their absence signals "incomplete tooling" to level designers comparing against Unity/Unreal.

| Feature | Why Expected | Complexity | Depends On | Notes |
|---------|--------------|------------|------------|-------|
| Delete key works when viewport is focused | Unity/Unreal both respond to Delete wherever focus is — currently only works when Outliner panel is focused | LOW | Existing `ImGuiKey_Delete` handling in Outliner | Move key check to global scope (viewport-focused or any editor window focused, not just Outliner); `ImGuiKey_Delete` already used, just needs scope expansion |
| Cmd+D / Ctrl+D duplicate selected object | Industry standard shortcut; Unity uses Ctrl+D, Unreal uses Ctrl+W for duplicate | LOW | `duplicateObject()` already exists on `EditorSceneDocument` | Wire keyboard shortcut to existing method + push undo command; duplicate should place copy slightly offset from original |
| Delete + Duplicate accessible from viewport right-click context menu | Both Unity and Unreal surface these in viewport right-click; prevents needing to hunt through panels | LOW | Selection state, existing menu patterns | Standard ImGui `BeginPopupContextItem` on viewport |
| Selection clearing with Escape or click-on-empty | Unity: Escape deselects; Unreal: clicking empty space deselects | LOW | Selection state (`selectedIds`) | Click-on-empty already partially handled; Escape key clear needs adding |
| Frame/focus selected object (F key) | Unity uses F in Scene view; Unreal uses F to frame selected — universally expected in 3D editors | LOW | `frameSelectionRequested` flag exists | Flag already exists; verify F key is bound in viewport input handling |
| Selection highlight: outline on hovered object | Unity shows orange outline on selected, different color on hovered; absence of hover feedback makes editors feel unresponsive | MEDIUM | Rendering pipeline (stencil/outline pass) | Requires stencil buffer or object-ID pass for hover; selection highlight may already exist for selected — hover is the gap |
| Add mesh from asset browser double-click (not just drag) | Unity: double-clicking an asset in Project window adds it to scene at world origin | LOW | `commitPlacement()`, content registry | Fallback for users who prefer click over drag; places at camera focus point or world origin |

### Differentiators (Competitive Advantage)

Features beyond parity that improve workflow velocity specifically for this project's solo developer usage pattern.

| Feature | Value Proposition | Complexity | Depends On | Notes |
|---------|-------------------|------------|------------|-------|
| Duplicate-in-place with auto-offset | Ctrl+D places duplicate at original position offset by a small amount (e.g. 0.5 units on X); eliminates the "where did my duplicate go?" confusion | LOW | `duplicateObject()` + transform offset logic | More ergonomic than Unity's same-position duplicate |
| Selection stays active after delete+undo | After Ctrl+Z undoes a delete, the previously deleted objects should be re-selected; many editors lose selection on undo | LOW | `restoreState()` + selection sync | Improves iterative delete/undo workflows |
| Selection cycling popup (pick behind overlapping meshes) | Already partially built via `EditorSelectionPickerState` — surface it as a popup menu when multiple objects are under the cursor | MEDIUM | `EditorSelectionPickerState`, `renderSelectionPicker()` | The "distracting selection overlay" bug the user mentioned — make this feel intentional and polished rather than broken |
| Viewport-focused keyboard shortcuts (global hotkeys) | All shortcuts work regardless of which panel is focused (as long as no text field is active) — mirrors Unreal's behavior | LOW | ImGui focus management, `WantTextInput` guard | The current Delete-only-in-Outliner limitation; generalize to a global shortcut dispatch layer |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Better Approach |
|---------|---------------|-----------------|-----------------|
| Box/marquee select (drag rectangle to select multiple) | Standard in Unity/Unreal | High complexity: requires separate interaction mode, hit testing against screen-space rectangle for all objects in viewport, conflicts with existing camera pan on mouse drag | Use Shift+click multi-select in outliner (already works) + Ctrl+click additive selection; marquee can be deferred to v2 |
| Copy-paste between scenes (Ctrl+C / Ctrl+V) | Seems intuitive — copy a door, paste it | Requires clipboard serialization format, handling of material/mesh ID references across scenes; cross-scene paste is ambiguous about world position | Duplicate (Ctrl+D) within same scene covers 90% of use cases; cross-scene copy is v2+ |
| Multi-object transform gizmo (single gizmo for multiple selected) | Unity/Unreal both support this | ImGuizmo `Manipulate()` operates on a single 4x4 matrix; multi-select gizmo requires computing a group pivot, applying delta transforms to each object, then decomposing — non-trivial with the existing single-object gizmo path | Move gizmo works on the primary selected object for now; multi-move can be a phase-2 improvement |
| Undo history panel (visual timeline) | Looks professional | Marginal DX gain over Ctrl+Z; significant UI surface area to build and maintain; the undo label in menu is sufficient | Surface undo/redo labels in Edit menu (already exists via `undoLabel()` / `redoLabel()`) |
| Asset drag from OS Finder/Explorer into viewport | Drag an FBX from Finder into the editor | Requires native OS drag-drop interop via GLFW/platform API — significant engineering; not worth the complexity for a solo project | Use the in-editor asset browser drag-to-viewport (already built) |
| Real-time multi-user collaboration | Some editors support this | Completely out of scope for a solo project engine | Single-writer scene file; no need |

---

## Feature Dependencies

```
[Delete key global shortcut]
    └──requires──> [Viewport focus detection (ImGui IsWindowFocused)]
    └──requires──> [WantTextInput guard (already used in Outliner)]

[Ctrl+D duplicate shortcut]
    └──requires──> [duplicateObject() on EditorSceneDocument (already exists)]
    └──requires──> [EditorCommandStack.pushDocumentStateCommand() (already exists)]
    └──requires──> [Transform offset after duplicate]

[Selection cycling popup polish]
    └──requires──> [EditorSelectionPickerState (already exists)]
    └──requires──> [renderSelectionPicker() (already exists)]
    └──enhances──> [viewport click selection UX]

[Viewport context menu (right-click)]
    └──requires──> [Selection state (selectedIds)]
    └──contains──> [Delete, Duplicate, Frame Selected]

[Frame selected (F key)]
    └──requires──> [frameSelectionRequested flag (already exists)]
    └──requires──> [Camera orbit / zoom to bounds logic in EditorViewportController]

[Global hotkey dispatch layer]
    └──enhances──> [Delete key, Ctrl+D, F key, Escape clear selection]
    └──prevents──> [Duplicate key handling across every panel separately]
```

### Dependency Notes

- **Ctrl+D requires duplicateObject():** The method already exists on `EditorSceneDocument`. The shortcut just needs wiring through the command stack with a before/after state capture. Complexity is very low — the infrastructure is complete.
- **Delete global requires focus management:** Currently `ImGuiKey_Delete` is checked inside `if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))` scoped to the Outliner. The fix is to move this check to the top-level editor update, guarded by `!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()`.
- **Selection cycling polish depends on existing infrastructure:** `EditorSelectionPickerState` and `renderSelectionPicker()` already exist. The "distracting overlay" the user mentioned is likely a UX refinement of the existing cycling behavior, not a new feature build.
- **Frame selected (F key) may already be wired:** `frameSelectionRequested` flag exists in `EditorUiState`. Verify whether the viewport input handling already sets this on F keypress before building it.

---

## MVP Definition

### Launch With (v1.1)

Minimum to call the editor "Unity/Unreal parity for basic workflows."

- [ ] **Delete key works when viewport is focused** — currently breaks the most fundamental expected behavior (press Delete to remove selected object while working in viewport)
- [ ] **Ctrl+D duplicate shortcut** — second most expected shortcut; `duplicateObject()` already exists, just needs wiring
- [ ] **Selection cycling popup polished** — the "distracting overlay" bug; make it feel intentional (popup menu) rather than broken stencil artifact
- [ ] **Escape clears selection** — nearly zero-complexity; standard UX expectation
- [ ] **Right-click context menu in viewport** — surfaces Delete, Duplicate, Frame Selected without requiring knowledge of keyboard shortcuts

### Add After Validation (v1.x)

- [ ] **Hover highlight on objects** — improves discoverability significantly but requires rendering pipeline work (stencil pass); worth adding after core manipulation UX is correct
- [ ] **Selection persists through undo of delete** — polish; depends on selection state being included in undo state or synced after redo

### Future Consideration (v2+)

- [ ] **Box/marquee selection** — valid feature but high complexity relative to benefit for solo developer; outliner multi-select is sufficient for now
- [ ] **Multi-object gizmo (group pivot)** — requires ImGuizmo extension or custom group-pivot math
- [ ] **Copy-paste across scenes** — requires clipboard serialization format

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Delete key global (not just outliner) | HIGH | LOW | P1 |
| Ctrl+D duplicate shortcut | HIGH | LOW | P1 |
| Selection cycling popup polish | HIGH | LOW (infrastructure exists) | P1 |
| Escape clears selection | MEDIUM | LOW | P1 |
| Right-click viewport context menu | MEDIUM | LOW | P1 |
| Frame selected (F key) | MEDIUM | LOW (flag exists, verify wiring) | P1 |
| Hover highlight on objects | MEDIUM | MEDIUM | P2 |
| Selection persists through undo | LOW | LOW | P2 |
| Box/marquee select | MEDIUM | HIGH | P3 |
| Multi-object group gizmo | MEDIUM | HIGH | P3 |
| Copy-paste across scenes | LOW | HIGH | P3 |

**Priority key:**
- P1: Required for v1.1 milestone
- P2: Add when core is done, before v1.2
- P3: Defer to v2+

---

## Competitor Feature Analysis

Unity and Unreal are the reference implementations. This editor targets parity on manipulation primitives, not feature completeness.

| Feature | Unity | Unreal Engine 5 | This Editor |
|---------|-------|-----------------|-------------|
| Delete selected | Delete key (any focused window) | Delete key | Delete in Outliner only — needs global |
| Duplicate | Ctrl+D | Ctrl+W | `duplicateObject()` exists, shortcut not wired |
| Copy/Paste | Ctrl+C / Ctrl+V | Ctrl+C / Ctrl+V | Not yet built |
| Undo/Redo | Ctrl+Z / Ctrl+Shift+Z | Ctrl+Z / Ctrl+Y | Built (256-step stack) |
| Frame selected | F (Scene view) | F (Viewport) | Flag exists; verify wiring |
| Clear selection | Escape / click empty | Escape / click empty | Partially (click empty); Escape not wired |
| Multi-select (additive) | Ctrl+click | Ctrl+click | Built in Outliner; verify in viewport |
| Multi-select (range) | Shift+click | Shift+click | Built in Outliner |
| Selection highlight | Orange outline | Yellow outline | Exists for selected; hover state TBD |
| Selection cycling (overlapping) | "Selection piercing menu" popup | Alt+click cycles | `EditorSelectionPickerState` exists |
| Translate gizmo | W key | W key | Built (ImGuizmo) |
| Rotate gizmo | E key | E key | Built (ImGuizmo) |
| Scale gizmo | R key | R key | Built (ImGuizmo) |
| Grid snapping | Snap to grid toggle | Ctrl disables snap | Built |
| Right-click context menu | Yes (Create, Delete, Duplicate) | Yes | Not yet built |
| Drag mesh from browser | Yes (Project window → Scene) | Yes (Content Browser → Viewport) | Built |
| Scene hierarchy (outliner) | Hierarchy panel | World Outliner | Built |
| Inspector panel | Inspector | Details Panel | Built |

---

## Sources

- Unity Manual: Pick and select GameObjects — https://docs.unity3d.com/Manual/ScenePicking.html (MEDIUM confidence — excerpt available)
- Unity Hotkeys reference — https://oxmond.com/unity-shortcuts/ (MEDIUM confidence — third-party verified against Unity docs structure)
- Unreal Engine Actor Selection — https://dev.epicgames.com/documentation/en-us/unreal-engine/level-editor-in-unreal-engine (HIGH confidence — official docs)
- Unreal Engine Viewport Controls — https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-controls-in-unreal-engine (HIGH confidence — official docs)
- Unreal Engine Keyboard Shortcuts blog — https://www.domestika.org/en/blog/4733-basic-keyboard-shortcuts-for-unreal-engine-4-and-5 (MEDIUM confidence)
- ImGuizmo GitHub — multi-select issue #150 — https://github.com/CedricGuillemet/ImGuizmo/issues/150 (HIGH confidence — library maintainer's own repo)
- Hazel Engine 2023.1 (ImGuizmo multi-select, Ctrl+D duplicate) — https://docs.hazelengine.com/HazelReleaseNotes/Hazel-2023.1 (MEDIUM confidence)
- Existing codebase: `src/editor/` — HIGH confidence (direct inspection)

---

*Feature research for: Level editor UX — professional scene object manipulation*
*Milestone: v1.1 Editor UX*
*Researched: 2026-04-01*
