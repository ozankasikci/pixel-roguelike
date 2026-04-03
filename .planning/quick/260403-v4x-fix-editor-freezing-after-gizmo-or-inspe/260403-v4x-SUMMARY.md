# Quick Task 260403-v4x Summary

## Task

Fix editor freezing after gizmo or inspector scene changes trigger runtime preview rebuild.

## Root Cause

The previous play-preview corruption fix intentionally marked ordinary `sceneRevision()` edits as `FullWorldRebuild`, but it reused the same idle background rebuild path that was meant for heavier structural `previewDirty` changes.

That meant a normal transform edit could do this:

1. user moves a mesh with the gizmo
2. editor keeps the live edit viewport in sync
3. runtime preview is marked stale
4. ~200ms later, the main editor loop runs a full `runtimePreviewSession.rebuild()` + `prewarmRenderer()`
5. the editor stalls and macOS shows the rainbow cursor

So the freeze was a regression from conflating:

- `must rebuild before next Play`
- `should eagerly rebuild right now while editing`

## What Changed

- In [apps/level_editor/main.cpp](/Users/ozan/Projects/gsd-3d-roguelike/apps/level_editor/main.cpp), I added a small `runtimePreviewAutoRebuildPending` flag.
- Structural `previewDirty` changes still mark `FullWorldRebuild` and allow the existing eager background rebuild path.
- Ordinary scene-revision edits from gizmo/inspector/undo/redo still mark `FullWorldRebuild`, but they explicitly disable eager background rebuild.
- Entering Play still rebuilds from the latest `EditorSceneDocument`, so the earlier stale-state fix remains intact.

## Verification

- `cmake --build build --target level-editor test_editor_runtime_preview`
- `ctest --test-dir build --output-on-failure -R editor_runtime_preview`

## Notes

- This fix is intentionally conservative: it restores editor responsiveness for ordinary scene edits without redesigning runtime-preview invalidation.
- Implementation commit: `ff7ce59` (`fix(editor): avoid blocking preview rebuild after scene edits`)
