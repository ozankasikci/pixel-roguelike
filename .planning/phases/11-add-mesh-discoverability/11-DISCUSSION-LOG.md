# Phase 11: Add Mesh Discoverability - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-01
**Phase:** 11-add-mesh-discoverability
**Areas discussed:** Button placement and UX flow, Mesh picker design, Placement behavior

---

## Button Placement and UX Flow

### Button Location

| Option | Description | Selected |
|--------|-------------|----------|
| Toolbar button | Prominent button in the top toolbar alongside transform tools. Always visible, one click away | ✓ |
| Asset browser panel header | Button at the top of the existing asset browser panel. Contextually placed near assets | |
| Both toolbar and panel | Primary toolbar button plus secondary shortcut in asset browser panel header | |

**User's choice:** Toolbar button
**Notes:** None

### Click Behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Opens a popup mesh picker | A modal/popup list of all available meshes appears. User selects one, then enters click-to-place mode | ✓ |
| Dropdown with mesh list | ImGui dropdown/combo directly from the button showing mesh names | |
| Enters placement mode with current mesh | Immediately enters placement mode with whatever mesh is currently selected | |

**User's choice:** Opens a popup mesh picker
**Notes:** None

---

## Mesh Picker Design

### Display Style

| Option | Description | Selected |
|--------|-------------|----------|
| Simple name list | Scrollable list of mesh names. Fast to scan, minimal implementation | ✓ |
| Categorized list | Mesh names grouped by folder or type. More organized but needs category logic | |
| Grid with 3D previews | Thumbnail grid showing rendered mesh previews. Best UX but requires offline rendering pipeline | |

**User's choice:** Simple name list
**Notes:** None

### Search/Filter

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, text filter | A text input at the top that filters the mesh list as you type | ✓ |
| No filter needed | The mesh list is small enough to scan | |
| You decide | Claude picks based on implementation complexity and mesh count | |

**User's choice:** Yes, text filter
**Notes:** None

---

## Placement Behavior

### Position

| Option | Description | Selected |
|--------|-------------|----------|
| Click-to-place | After selecting from picker, enter placement mode. User clicks viewport to position | ✓ |
| Drop at camera center | Mesh immediately appears in front of the camera. No click needed | |
| Drop at world origin | Mesh appears at (0,0,0) | |

**User's choice:** Click-to-place
**Notes:** Reuses existing EditorPlacementState/beginPlacement system

### Material

| Option | Description | Selected |
|--------|-------------|----------|
| Current selected material | Uses ui.selectedMaterialId. Matches existing Place Mesh behavior | ✓ |
| You decide | Claude picks the most sensible default approach | |

**User's choice:** Current selected material
**Notes:** None

---

## Claude's Discretion

- Exact ImGui window/popup style for the picker
- Button label text and icon
- How mesh names are derived from asset paths
- Whether picker auto-closes on selection or stays open

## Deferred Ideas

None — discussion stayed within phase scope.
