# Phase 11: Add Mesh Discoverability - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

A clearly labeled "Add Mesh" button in the editor toolbar opens a popup mesh picker showing all available meshes in the project. Selecting a mesh enters click-to-place mode (reusing the existing placement system). The placed mesh appears in viewport and outliner, is auto-selected, and persists on save.

</domain>

<decisions>
## Implementation Decisions

### Button Placement and UX Flow
- **D-01:** "Add Mesh" button lives in the top toolbar, alongside existing transform tool buttons — always visible, one click away
- **D-02:** Clicking the button opens a popup mesh picker (not a dropdown, not immediate placement mode)

### Mesh Picker Design
- **D-03:** Simple scrollable name list of all available meshes — no thumbnails, no grid, no 3D previews
- **D-04:** Text filter input at the top of the picker popup — filters the mesh list as user types

### Placement Behavior
- **D-05:** After selecting a mesh from the picker, enter click-to-place mode using the existing `beginPlacement(placementState, EditorPlacementKind::Mesh, ...)` system
- **D-06:** Use the currently selected material (`ui.selectedMaterialId`) for the placed mesh — matches existing Place Mesh behavior

### Claude's Discretion
- Exact ImGui window/popup style for the picker (modal vs non-modal popup)
- Button label text and icon (if any)
- How mesh names are derived from asset paths (filename, meshId, or display name)
- Whether the picker popup auto-closes on selection or stays open for multi-place

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Asset Browser Infrastructure
- `src/editor/assets/EditorAssetBrowser.h` — `EditorAssetBrowserNode`, `buildProjectAssetBrowserTree()`, mesh scanning
- `src/editor/assets/EditorAssetBrowser.cpp` — Asset tree building, mesh ID extraction
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — Existing asset browser panel UI rendering

### Placement System
- `src/editor/viewport/EditorViewportInteraction.h` — `EditorPlacementState`, `beginPlacement()`, `commitPlacement()`
- `src/editor/viewport/EditorViewportInteraction.cpp` — Placement state management and commit logic
- `apps/level_editor/main.cpp:487-513` — `renderCreateCommands()` lambda — existing "Place Mesh" menu item

### Editor UI State
- `src/editor/ui/LevelEditorUi.h` — `EditorUiState` (selectedMeshId, selectedMaterialId), `EditorPlacementState`, `EditorPlacementKind`

### Toolbar and Layout
- `apps/level_editor/main.cpp` — Main editor loop, toolbar rendering, ImGui layout

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `buildProjectAssetBrowserTree()` — Already scans `assets/` and returns a tree of `EditorAssetBrowserNode` with `kind == Mesh` filtering available
- `beginPlacement()` — Existing function to start click-to-place mode; takes placement kind, meshId, materialId
- `commitPlacement()` — Handles the actual placement commit when user clicks in viewport
- `EditorPlacementState` — Tracks active placement mode; `.active()`, `.clear()` methods
- `renderCreateCommands()` — Lambda in main.cpp with existing "Place Mesh" menu item pattern

### Established Patterns
- Toolbar buttons use ImGui::Button() with text labels in the main toolbar area
- Placement mode is activated via `beginPlacement()` and cleared with `placementState.clear()`
- Mesh IDs come from `EditorAssetBrowserNode::meshId` field
- Asset scanning uses `buildProjectAssetBrowserTree()` which returns the full asset tree

### Integration Points
- Toolbar area in main.cpp (after transform tool buttons) — insert "Add Mesh" button
- `renderCreateCommands()` lambda — existing mesh placement trigger to reference
- `EditorUiState::selectedMeshId` — updated when user picks a mesh from the picker

</code_context>

<specifics>
## Specific Ideas

- The button should be clearly labeled so a new user can discover mesh placement without reading docs or knowing shortcuts
- Text filter should be responsive (filter on every keystroke, not on Enter)
- The mesh list should use the same mesh IDs that the existing placement system uses

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 11-add-mesh-discoverability*
*Context gathered: 2026-04-01*
