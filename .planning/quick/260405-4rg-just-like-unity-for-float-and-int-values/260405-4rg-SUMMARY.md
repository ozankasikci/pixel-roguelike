# Quick Task 260405-4rg: Unity-style drag precision for float/int inspector fields

## Summary

Reduced the default drag speed for `editVec3` from `0.1f` to `0.01f`, giving 10x finer precision when dragging numeric fields in the inspector. This matches the sensitivity already used by environment/lighting panel fields.

## Changes

| File | Change |
|------|--------|
| `src/editor/ui/LevelEditorUi.h` | Changed `editVec3` default speed parameter from `0.1f` to `0.01f` |

## Context

ImGui's `DragFloat` family already supports Unity-style drag-on-label natively — hovering over the label shows a resize cursor, and dragging adjusts the value. The only issue was the speed parameter being too coarse at `0.1f` for Position and other Vec3 fields that used the default. Fields with explicit speeds (Scale at `0.02f`, Rotation at `0.5f`, environment values at `0.01f`) were unaffected.

## Verification

- Build: level-editor compiles clean
- Manual: user confirmed drag-on-label works, requested finer precision
