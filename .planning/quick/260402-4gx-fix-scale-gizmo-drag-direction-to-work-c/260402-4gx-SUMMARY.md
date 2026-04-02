---
phase: quick
plan: 260402-4gx
subsystem: editor/gizmo
tags: [imguizmo, gizmo, scale, ux]
key-files:
  modified:
    - external/ImGuizmo/ImGuizmo.cpp
decisions:
  - Use mScreenSquareCenter (already computed per-frame in ImGuizmo) as the radial origin — no new state needed beyond mSaveMousePosy
  - Keep 0.01f sensitivity identical to original for consistent feel
  - Fallback to horizontal-only delta for degenerate case (user clicks exactly on object center)
metrics:
  duration: 5m
  completed: "2026-04-02"
  tasks_completed: 1
  files_modified: 1
---

# Quick Task 260402-4gx: Fix Scale Gizmo Drag Direction Summary

**One-liner:** Unity-style radial uniform scale in ImGuizmo — dragging outward from object center increases scale, inward decreases, using screen-space projection onto the click-to-center direction vector.

## What Was Done

Modified `external/ImGuizmo/ImGuizmo.cpp` to replace the horizontal-only uniform scale delta with a screen-space radial projection approach matching Unity's behavior.

### Changes

**1. Added `mSaveMousePosy` field** (line ~728 in Context struct)

Added `float mSaveMousePosy;` right after the existing `float mSaveMousePosx;` to track the initial Y position when a scale drag starts.

**2. Store initial Y on drag start** (line ~2316)

Added `gContext.mSaveMousePosy = io.MousePos.y;` in the HandleScale drag-start block alongside the existing X storage.

**3. Replaced uniform scale computation** (lines ~2345-2369)

Replaced:
```cpp
float scaleDelta = (io.MousePos.x - gContext.mSaveMousePosx) * 0.01f;
gContext.mScale.Set(max(1.f + scaleDelta, 0.001f));
```

With radial projection: compute the direction vector from `mScreenSquareCenter` (object center in screen space) to `mSaveMousePos` (initial click point), normalize it, then project the current mouse delta onto that direction. The projected scalar is the scale delta.

## Approach: Why Radial Projection

`mScreenSquareCenter` is the screen-space projection of the object's origin — already computed every frame in ImGuizmo's `ComputeContext`. Using the direction from center to click as the "outward" axis means:

- The user's intent (move toward or away from the object) is correctly interpreted
- Works identically at any camera angle — the mapping is purely screen-space
- No 3D world-space knowledge needed for uniform scale

## Build Verification

```
[100%] Building CXX object src/editor/CMakeFiles/editor.dir/__/__/external/ImGuizmo/ImGuizmo.cpp.o
[100%] Built target level-editor
```

No errors or warnings.

## Commits

| Hash | Description |
|------|-------------|
| 8b1e428 | Fix scale gizmo uniform drag to use Unity-style screen-space radial direction |

## Manual Verification Required (Checkpoint)

The task includes a `checkpoint:human-verify` gate. Per execution constraints, automated parts are complete. Manual steps to verify:

1. Launch `./build/apps/level-editor/level-editor`
2. Open any scene, select an object, press R for Scale tool
3. Grab the center cube handle (uniform scale) and drag:
   - Drag OUTWARD from the object — scale should INCREASE
   - Drag INWARD toward the object — scale should DECREASE
4. Test with the object at different screen positions (left, right, top, bottom of viewport)
5. Verify per-axis colored handles (X/Y/Z) still work correctly

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check

- [x] `external/ImGuizmo/ImGuizmo.cpp` modified
- [x] Commit `8b1e428` exists
- [x] `level-editor` builds successfully
