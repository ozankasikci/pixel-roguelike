# Quick Task 260403-udy Summary

## Task

Fix play preview texture corruption after moving a scene mesh with the gizmo.

## What Changed

- In [apps/level_editor/main.cpp](/Users/ozan/Projects/gsd-3d-roguelike/apps/level_editor/main.cpp), scene-revision-only editor changes now mark the runtime play preview as `FullWorldRebuild` instead of trying to keep reusing the existing runtime session.
- The edit viewport still gets its live incremental sync through `previewWorld.syncTransforms()`, `syncMaterials()`, and `syncLights()`.
- The old `runtimePreviewSession.syncMaterials()` call was removed from that scene-revision path so the runtime preview no longer mutates live meshes against a stale world after gizmo moves.
- In [tests/editor/test_editor_runtime_preview.cpp](/Users/ozan/Projects/gsd-3d-roguelike/tests/editor/test_editor_runtime_preview.cpp), I added a regression that mutates a mesh in the editor document, rebuilds the runtime preview, renders, and asserts the rebuilt runtime mesh picked up the moved position, material, and tint by node id.

## Why This Fix

The bug path was in the editor/runtime split:

- moving a mesh increments `document.sceneRevision()`
- the edit preview world updated live
- the runtime play preview session was still being reused
- entering Play could then render stale runtime mesh state

This quick fix chooses correctness over partial incremental sync: any scene-object revision now takes the existing full runtime rebuild path before Play Preview is used again.

## Verification

- `cmake --build build --target level-editor test_editor_runtime_preview`
- `ctest --test-dir build --output-on-failure -R editor_runtime_preview`
- `./build/tests/editor/test_editor_runtime_preview`

## Notes

- Implementation commit: `9d1ee2b` (`fix(editor): rebuild play preview after scene mesh edits`)
