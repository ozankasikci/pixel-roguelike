# Project Research Summary

**Project:** 3D Roguelike — Level Editor v1.1 UX Milestone
**Domain:** Custom C++ game engine level editor — professional object manipulation UX
**Researched:** 2026-04-01
**Confidence:** HIGH

## Executive Summary

This milestone targets Unity/Unreal parity for fundamental scene-editing operations in the custom C++ level editor. The critical finding from all four research streams is that the vast majority of the required functionality is already built. Delete, duplicate, add mesh, gizmo transform, multi-select, undo/redo, drag-drop placement, and the selection cycling popup all have complete implementations in the existing codebase. The v1.1 milestone reduces almost entirely to wiring work (connecting existing methods to missing keyboard shortcuts) and one rendering fix (the selection overlay depth problem).

The recommended approach is: fix the one real rendering gap first (selection overlay respecting depth), then audit keyboard shortcut routing to make Delete and Ctrl+D work globally across all focused panels rather than only in the Outliner, then surface operations in a right-click context menu. No new library dependencies are required. Every tool needed — ImGui's shortcut system, the OpenGL stencil buffer, the existing `duplicateObject()` and `eraseObjects()` APIs — is already present.

The primary risk is not capability gaps but correctness of new wiring. Every new code path that mutates `EditorSceneDocument` must follow the existing capture-before/push-after undo pattern, shortcuts must be guarded against firing inside `InputText` fields, and duplicated objects must receive distinct node IDs to survive serialization round-trips. These pitfalls are well-understood, each has a clear prevention pattern documented in the codebase, and the recovery cost when caught early is low (under a day per pitfall).

---

## Key Findings

### Recommended Stack

No new dependencies are needed. The full capability set for this milestone is covered by libraries already vendored or installed: Dear ImGui v1.92.6-docking (shortcut routing, `BeginMultiSelect`, key ownership fixes), ImGuizmo v1.92.5 WIP (gizmo transform), and OpenGL 4.1 Core Profile (stencil buffer for depth-correct selection outlines). The stack is stable and internally consistent — ImGuizmo version-matches to the ImGui internals it is vendored against.

**Core technologies already in use:**
- **Dear ImGui v1.92.6-docking**: keyboard shortcut routing via `ImGui::Shortcut()` with `ImGuiInputFlags_RouteFocused` / `RouteGlobal` — the right API for global vs. panel-scoped shortcuts; ImGui issue #8048 (InputText Delete key ownership) is fixed in this version
- **ImGuizmo v1.92.5 WIP**: translate/rotate/scale gizmo already wired in `EditorViewportController.cpp`; single-object transform is complete; multi-object requires group-pivot extension (deferred to v2+)
- **OpenGL 4.1 stencil buffer**: two-pass stencil technique for depth-correct selection outline — primary pass with depth testing enabled, second pass with `GL_NOTEQUAL` stencil test for the outline; no additional library needed
- **EnTT v3.16.0**: ECS preview world mirrors document state; no changes needed for this milestone

### Expected Features

**Must have (table stakes — v1.1):**
- Delete key works when viewport is focused, not just Outliner — the single most disruptive UX gap relative to Unity/Unreal
- Ctrl+D duplicate shortcut — `duplicateObject()` exists; only shortcut wiring is missing
- Selection overlay fixed: depth-correct highlight that does not bleed through occluding geometry
- Escape clears selection — near-zero complexity; universally expected in 3D editors
- Right-click context menu in viewport — surfaces Delete, Duplicate, Frame Selected without requiring shortcut memorization

**Should have (competitive — v1.x after core is validated):**
- Hover highlight on unselected objects — requires the same stencil pipeline as the selection fix; defer until that pass is stable
- Selection persists through undo of delete — polish; requires syncing `selectedIds` from restored state or re-selecting by ID after undo

**Defer (v2+):**
- Box/marquee selection — high complexity, conflicts with camera pan on drag; outliner multi-select covers the use case
- Multi-object gizmo (group pivot) — ImGuizmo single-matrix API needs extension; not blocking any current workflow
- Copy-paste across scenes — requires clipboard serialization format and cross-scene ID resolution

### Architecture Approach

The editor follows a clean orchestrator pattern: `apps/level_editor/main.cpp` owns all state and dispatches mutations at end-of-frame after all panels have rendered. UI panels return action result structs rather than mutating the document directly. This deferred-action pattern is the critical architectural invariant — violating it (mutating document mid-render) causes stale-state bugs across panels in the same frame.

**Major components relevant to this milestone:**
1. `EditorSceneDocument` — authoritative scene graph; all mutations go through its API (`eraseObjects`, `duplicateObject`, `addMesh`, `applyWorldTransform`); the API is complete, nothing new needed
2. `EditorCommandStack` — 256-step snapshot-based undo/redo; requires explicit capture-before/push-after per mutation; no automatic mutation hook; every new entry point must re-implement this pattern
3. `EditorPreviewWorld` — ECS mirror for rendering; full rebuild on `previewDirty=true`; cheap material/light sync via revision counters for transform-only changes; set `previewDirty=true` only on structural mutations (add/remove/mesh-ID change)
4. `appendSelectionOverlays()` in `EditorScenePreviewRenderer.cpp` — the single file to change for the depth fix; currently sets `ignoreDepth=true` on all selection wireframes, causing the bleed-through issue

### Critical Pitfalls

1. **Delete fires inside InputText** — `!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()` guard is required on every shortcut check. The outliner already uses it correctly; new viewport shortcut paths must replicate this pattern. Without it, typing in the inspector's node-name field and pressing Delete removes the scene object.

2. **Missing undo push on new mutation paths** — Every new entry point for Delete, Duplicate, or Add Mesh must independently capture before-state and call `commandStack.pushDocumentStateCommand()`. The pattern is not enforced by the type system. A right-click context menu that calls `eraseObjects()` without the surrounding capture-push pair silently makes deletions non-undoable.

3. **Stale `selectedIds` after undo/redo** — `selectedIds` lives outside `EditorSceneDocumentState` and is not restored by undo. `pruneSelection(document, selectedIds)` must be called immediately after every `commandStack.undo()` and `commandStack.redo()`. Inspector crashes (`findObject()` returning nullptr) are the symptom.

4. **Duplicate produces colliding node IDs** — `duplicateObject()` copies the payload verbatim including the `nodeId` field. The duplicate gets the same node ID as the original, causing silent overwrite on the next serialization round-trip (the scene has N duplicates at edit time but N/2 on reload). `ensureObjectNodeId()` must be called on the duplicate after payload copy.

5. **Shortcut fires in both viewport and outliner simultaneously** — ImGui docking makes `ImGuiFocusedFlags_RootAndChildWindows` ambiguous across docked siblings. Global shortcuts (Delete, Ctrl+D, Ctrl+Z) must live in `main.cpp` as single authoritative handlers using `ImGui::Shortcut(..., ImGuiInputFlags_RouteGlobal)`, not inside individual panel render functions.

---

## Implications for Roadmap

Based on combined research, the work naturally splits into three focused phases with clear ordering based on dependencies and risk.

### Phase 1: Selection Overlay Depth Fix

**Rationale:** This is the only change requiring a rendering pipeline modification. It must come first because all subsequent testing (viewport context menu, hover highlight) depends on correct visual feedback for selections. It is self-contained — a single-file change with no dependencies on later work.

**Delivers:** Selection wireframe that respects occluding geometry. Primary wireframe pass with `ignoreDepth=false` for correct occlusion; optional secondary faint pass (`ignoreDepth=true`, ~25% opacity) as an occluded-hint visible when the selected object is fully behind a wall.

**Addresses:** FEATURES "selection highlight" table-stakes item; the named visual bug in the v1.1 milestone goal.

**Avoids:** Pitfall 8 (overlay bleeds through walls) — change `appendSelectionOverlays()` in `EditorScenePreviewRenderer.cpp` to remove `ignoreDepth=true` from the primary pass.

**File changed:** `src/editor/render/EditorScenePreviewRenderer.cpp` only — no other files affected.

**Research flag:** Standard OpenGL stencil/depth pattern, well-documented — skip research-phase.

---

### Phase 2: Global Keyboard Shortcut Routing

**Rationale:** Delete and Ctrl+D both need to be promoted from panel-scoped to global shortcuts in `main.cpp`. These features share the same underlying problem (shortcut scope) and the same fix pattern (`ImGui::Shortcut` with `RouteGlobal`, guarded by `!WantTextInput`). Doing them together avoids auditing shortcut routing twice. Escape clear selection and F-key frame-selected can be verified or wired in the same pass.

**Delivers:** Delete works when viewport is focused (not just Outliner); Ctrl+D duplicate fires exactly once per keypress from any panel; Escape clears selection; F-key frame-selected verified or wired; duplicate places offset copy with selection transferred to the new object.

**Addresses:** FEATURES P1 items: Delete global, Ctrl+D, Escape clear selection, F-key frame selected.

**Avoids:**
- Pitfall 1 (Delete fires in InputText) — `WantTextInput` guard on all new shortcut paths
- Pitfall 2 (missing undo push) — duplicate shortcut path must include capture-before/push-after
- Pitfall 3 (stale selection after undo) — `pruneSelection` called after every undo/redo
- Pitfall 4 (duplicate node ID collision) — `ensureObjectNodeId()` called on duplicate payload
- Pitfall 5 (duplicate lands on top of original) — apply world-space offset via `computePlacementPoint()` or fixed camera-forward offset
- Pitfall 10 (shortcut fires twice) — single global handler in `main.cpp`; remove any panel-side duplicates for promoted shortcuts

**Files changed:** `apps/level_editor/main.cpp` (global shortcut dispatch); possibly `src/editor/ui/EditorOutlinerPanel.cpp` if consolidating the panel-side Delete check.

**Research flag:** Standard ImGui routing patterns, well-documented — skip research-phase.

---

### Phase 3: Viewport Right-Click Context Menu

**Rationale:** Builds on Phase 2 (needs Delete and Duplicate to work correctly first) and Phase 1 (needs correct visual state to meaningfully show selections). This is pure UI surface — no new document mutations, no new shortcut logic, just exposing existing operations in a discoverable menu. Completes the Unity/Unreal parity checklist for basic manipulation workflows.

**Delivers:** Right-click in viewport shows context menu with Delete, Duplicate, Frame Selected. Menu items invoke the same Phase 2 dispatch paths, ensuring undo tracking is automatic.

**Addresses:** FEATURES "right-click context menu" table-stakes item.

**Avoids:**
- Pitfall 2 (missing undo push) — menu items invoke existing Phase 2 dispatch paths, not direct document calls
- Pitfall 3 (stale selection) — `pruneSelection` already called from Phase 2 undo handlers

**Files changed:** `apps/level_editor/main.cpp` or viewport panel — `ImGui::BeginPopupContextWindow()` on the viewport window.

**Research flag:** Standard ImGui popup pattern — skip research-phase.

---

### Phase Ordering Rationale

- Phase 1 before Phase 2/3: the visual fix is fully independent and unblocks correct visual feedback during shortcut testing in subsequent phases
- Phase 2 before Phase 3: the context menu must invoke the same mutation paths as the keyboard shortcuts; build and validate those paths first, then expose them via menu
- All three phases avoid touching `EditorSceneDocument` internals — the document API is complete; all work is at the caller layer in `main.cpp` and the renderer

### Research Flags

All three phases use standard, well-documented patterns. No phases require deeper domain research before execution:

- **Phase 1:** OpenGL stencil two-pass outline is a textbook technique (LearnOpenGL); all OpenGL state calls already exist in the `Renderer`
- **Phase 2:** ImGui shortcut routing is fully documented in the vendored source; `WantTextInput` guard pattern is already used correctly in the outliner
- **Phase 3:** `ImGui::BeginPopupContextWindow()` is standard ImGui usage; no novel integration

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All library versions confirmed against GitHub releases and direct codebase inspection (`CMakeLists.txt`, `external/ImGuizmo/ImGuizmo.h`); no new dependencies needed |
| Features | HIGH | Cross-verified against Unity Manual, Unreal Engine official docs, and direct codebase inspection of `src/editor/`; existing vs. missing features clearly delineated with file-level evidence |
| Architecture | HIGH | All findings from direct source code inspection of `main.cpp`, `EditorSceneDocument`, `EditorCommandStack`, and related files; no training-data assumptions; all file paths and method names confirmed |
| Pitfalls | HIGH | Based on direct code inspection plus ImGui/ImGuizmo issue tracker (issues #8048, #6621, #292) plus community post-mortems; specific line numbers and prevention patterns cited |

**Overall confidence:** HIGH

### Gaps to Address

- **F-key frame-selected wiring**: `frameSelectionRequested` flag exists in `EditorUiState`; both ARCHITECTURE.md and FEATURES.md note "verify whether F key is already bound in viewport input handling." Audit at the start of Phase 2 before building anything new — it may already be wired.

- **`pruneSelection` call audit**: PITFALLS.md identifies that `pruneSelection` must be called after every undo/redo. Confirm it is present in the existing undo/redo dispatch in `main.cpp` before Phase 2 wires additional mutation paths that depend on selection state being valid.

- **Hover highlight scope**: FEATURES.md calls hover highlight P2. The stencil pipeline needed is the same as Phase 1, but hover state requires per-frame hit detection (a mouse-over raycast or separate hover ID). Not analyzed in detail — assess during Phase 1 whether the hover ID can be cheaply derived from `EditorSelectionPickerState` or needs a new per-frame raycast.

---

## Sources

### Primary (HIGH confidence)

- Existing codebase (direct inspection): `apps/level_editor/main.cpp`, `src/editor/scene/EditorSceneDocument.h/.cpp`, `src/editor/core/EditorCommand.h/.cpp`, `src/editor/viewport/EditorViewportInteraction.h/.cpp`, `src/editor/render/EditorScenePreviewRenderer.h/.cpp`, `src/editor/ui/EditorOutlinerPanel.cpp` — all claims verified against current source
- Dear ImGui GitHub v1.92.6-docking — https://github.com/ocornut/imgui — version and API confirmed; `BeginMultiSelect` shipped in 1.91.0 (Jul 2024)
- ImGui issue #8048 — InputText Delete key ownership fix — https://github.com/ocornut/imgui/issues/8048 — confirmed fixed in vendored version
- ImGuizmo GitHub master — last commit Dec 27, 2025 (v1.92.5 WIP) — https://github.com/CedricGuillemet/ImGuizmo/commits/master
- Unreal Engine Actor Selection and Viewport Controls — https://dev.epicgames.com/documentation/en-us/unreal-engine/level-editor-in-unreal-engine

### Secondary (MEDIUM confidence)

- LearnOpenGL Stencil Testing — two-pass outline technique — https://learnopengl.com/Advanced-OpenGL/Stencil-testing
- Unity Scene Picking Manual — https://docs.unity3d.com/Manual/ScenePicking.html
- ImGuizmo issue #292 — drawing elements behind geometry — https://github.com/CedricGuillemet/ImGuizmo/issues/292
- GameDev.net — custom editor undo/redo system — https://www.gamedev.net/forums/topic/678496-custom-editor-undoredo-system/5290700/
- Wolfire Games blog — How We Implement Undo — http://blog.wolfire.com/2009/02/how-we-implement-undo/
- Unity Hotkeys reference — https://oxmond.com/unity-shortcuts/ — cross-verified against Unity docs structure
- Hazel Engine 2023.1 release notes (ImGuizmo multi-select, Ctrl+D) — https://docs.hazelengine.com/HazelReleaseNotes/Hazel-2023.1

---

*Research completed: 2026-04-01*
*Ready for roadmap: yes*
