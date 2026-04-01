---
quick_task: 260402-2lb
title: Fix multi-object scaling gizmo losing precise control
date: 2026-04-02
commit: b9a2d31
tags: [editor, gizmo, multi-select, transform, bug-fix]
key-files:
  modified:
    - src/editor/viewport/EditorViewportInteraction.h
    - src/editor/viewport/EditorViewportInteraction.cpp
    - apps/level_editor/main.cpp
decisions:
  - MultiGizmoState caches centroid and transforms on first frame of each drag; cleared when manipulateEditorGizmo returns false
  - Single-object path unchanged — ImGuizmo overwrites matrix directly each frame, no compounding issue there
  - multi-object path now enabled (previously returned false for selectedIds.size() != 1)
---

# Quick Task 260402-2lb: Fix multi-object scaling gizmo losing precise control

**One-liner:** Cache original transforms at drag start in MultiGizmoState so multi-object gizmo applies absolute delta each frame instead of compounding.

## Root Cause

When multiple objects were selected and the scale (or translate/rotate) gizmo was dragged, the function `applyGizmoToSelectedObject` returned `false` immediately for multi-object selections (`selectedIds.size() != 1`). This meant multi-object gizmo interaction was completely non-functional before this fix.

The plan also identified the compounding issue: if a naive multi-object implementation re-read the current world transform each frame and applied a relative delta, scale would compound (1.2x on frame 1, then 1.2 * 1.3 = 1.56x instead of 1.3x on frame 2).

## Fix

Added `MultiGizmoState` struct to `EditorViewportInteraction.h`:
- `active` bool to track whether a drag is in progress
- `cachedCentroid` — pivot point frozen at drag start to prevent drift
- `cachedTransforms` — original world matrices per object ID (for Mesh, Collider, Archetype, PlayerSpawn)
- `cachedLightData` — original position/direction per light ID
- `clear()` method resets all state

Rewrote `applyGizmoToSelectedObject` to:
1. **Single-object path** (unchanged logic): ImGuizmo directly overwrites the matrix, no compounding possible.
2. **Multi-object path** (new): Places gizmo at `cachedCentroid`, computes `localDelta = invBefore * gizmoMatrix`, applies `before * localDelta * invBefore * cachedTransform[id]` to each object every frame — always from the cached original, never from the already-modified transform.

Updated `main.cpp` to declare `MultiGizmoState multiGizmoState` next to `gizmoCommand` and pass it to `applyGizmoToSelectedObject`.

## Verification

Build: `cmake --build build --target level-editor` — succeeded with no errors or warnings.

Manual test steps:
1. Open level editor, select 2+ mesh objects
2. Use scale gizmo: drag slowly — scale should track mouse precisely
3. Use translate gizmo with multi-select: objects should move together from their original positions
4. Release drag, re-drag: new drag caches fresh originals, no accumulated error

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED

- `src/editor/viewport/EditorViewportInteraction.h` — modified (MultiGizmoState struct + updated signature)
- `src/editor/viewport/EditorViewportInteraction.cpp` — modified (multi-object path rewritten)
- `apps/level_editor/main.cpp` — modified (multiGizmoState declared and passed)
- Commit `b9a2d31` exists
