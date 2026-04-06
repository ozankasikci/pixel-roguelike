---
phase: quick
plan: 260406-qr6
subsystem: level-loading
tags: [scene-format, doors, scripted-geometry, data-driven]
dependency_graph:
  requires: []
  provides: [native-single-door-scene-format, initial-scene-doors]
  affects: [LevelDef, LevelBuilder, LevelLoader, GenericFileScene, EditorRuntimePreviewSession]
tech_stack:
  added: []
  patterns: [single_door scene format record, keyword-value token parsing for doors]
key_files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.h
    - src/game/level/LevelLoader.cpp
    - src/game/scenes/GenericFileScene.h
    - src/game/scenes/GenericFileScene.cpp
    - src/editor/core/EditorRuntimePreviewSession.cpp
    - apps/runtime/main.cpp
    - apps/level_editor/main.cpp
    - src/game/CMakeLists.txt
    - assets/scenes/initial_scene.scene
  deleted:
    - src/game/scenes/InitialSceneScripted.cpp
    - src/game/scenes/InitialSceneScripted.h
decisions:
  - "single_door uses keyword-value token loop (same pattern as mesh/collider) with positional required fields and omit-defaults serialization"
  - "LevelSingleDoorPlacement mirrors SingleDoorSpawnSpec 1:1 plus nodeId/parentNodeId for hierarchy support"
  - "addSingleDoor in LevelBuilder bridges placement struct to spawnSingleDoor via spec conversion"
  - "buildScriptedGeometry removed entirely from LevelLoadRequest — doors now come from .scene file"
metrics:
  duration: ~20 minutes
  completed: 2026-04-06
  tasks_completed: 2
  files_changed: 15
---

# Phase quick Plan 260406-qr6: Remove Scripted Geometry Summary

**One-liner:** Native `single_door` record added to .scene format; all three InitialSceneScripted doors migrated to initial_scene.scene; scripted geometry infrastructure fully removed.

## Tasks Completed

| Task | Name | Commit | Key Files |
|------|------|--------|-----------|
| 1 | Add native single_door support to .scene format | 6823417 | LevelDef.h/cpp, LevelBuilder.h/cpp, LevelLoader.cpp |
| 2 | Migrate doors to scene file and remove all scripted geometry | 402f2bc | initial_scene.scene, GenericFileScene, EditorRuntimePreviewSession, CMakeLists.txt |

## What Was Built

Extended the .scene file format with a native `single_door` record type. The format supports all `SingleDoorSpawnSpec` fields as keyword-value pairs with positional required fields (doorMesh, frameMesh, position xyz, yaw). Omit-defaults serialization keeps scene files concise.

Three doors from `InitialSceneScripted.cpp` migrated to `initial_scene.scene`:
- Door A: west wall, `SM_DoorA/SM_FrameA`, yaw=90, qdp_door_a material
- Door B: north wall, `SM_DoorD/SM_FrameD`, yaw=180, open_duration=1.8, grayish metal tint
- Door C: east wall, `SM_DoorC/SM_FrameC`, yaw=-90, open_duration=1.0, off-white tint

The entire scripted geometry infrastructure was removed: `InitialSceneScripted.cpp/.h` deleted, `GenericFileScene` registry stripped, `EditorRuntimePreviewSession` lookup removed, `buildScriptedGeometry` field removed from `LevelLoadRequest`.

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None — all three doors are fully wired to the scene file and parsed through LevelDef/LevelLoader/LevelBuilder into `spawnSingleDoor`.

## Self-Check: PASSED

- FOUND: src/game/level/LevelDef.h
- FOUND: src/game/level/LevelBuilder.h
- FOUND: assets/scenes/initial_scene.scene (contains 3 single_door entries)
- DELETED: src/game/scenes/InitialSceneScripted.cpp (correct)
- DELETED: src/game/scenes/InitialSceneScripted.h (correct)
- FOUND commit: 6823417 — Add native single_door support to .scene format
- FOUND commit: 402f2bc — Migrate doors to scene file and remove all scripted geometry
- Build: PASSED (both pixel-roguelike and level-editor targets)
- grep InitialSceneScripted src/ apps/: 0 matches
- grep registerScriptedGeometry/lookupScriptedGeometry src/: 0 matches
- grep buildScriptedGeometry src/: 0 matches
