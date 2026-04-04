# Quick Task 260404-omb Summary

**Task:** Fix editor light gizmo moving helper but not preview light
**Date:** 2026-04-04
**Code Commit:** `1464a2a`
**Status:** Complete

## What changed

- Identified that the single-selection light gizmo path updated `LevelLightPlacement` directly without calling `document.markSceneDirty()`.
- Added the missing dirty/revision bump in `src/editor/viewport/EditorViewportInteraction.cpp` so preview-world light sync runs after the gizmo mutates a light.

## Why the bug happened

- The gizmo helper reads from the editor document payload, so it moved immediately.
- The rendered preview light reads from `EditorPreviewWorld`, which only refreshes lights when `document.sceneRevision()` changes.
- Because the single-light gizmo path did not advance that revision, the helper moved but the actual light in the scene stayed visually unchanged.

## Verification

- `cmake --build build --target test_editor_scene_document level-editor`
- `./build/tests/editor/test_editor_scene_document`

## Notes

- This fix covers single-selection light gizmo edits, including point/spot position updates and spot/directional direction changes.
- There is no dedicated automated regression for ImGuizmo light dragging yet; the repo's existing targeted editor test binary still passed after the patch.
