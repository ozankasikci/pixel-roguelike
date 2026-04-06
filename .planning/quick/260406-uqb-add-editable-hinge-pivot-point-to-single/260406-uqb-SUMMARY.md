---
phase: quick
plan: 260406-uqb
subsystem: doors / editor inspector
tags: [door, hinge, inspector, serialization, data-driven]
dependency_graph:
  requires: []
  provides: [hingePivot-field-on-single-door]
  affects: [level-editor, runtime-gameplay, scene-serialization]
tech_stack:
  added: []
  patterns: [data-driven-hinge-pivot, optional-serialization]
key_files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/prefabs/GameplayPrefabData.h
    - src/game/level/LevelBuilder.cpp
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/ui/inspectors/SingleDoorInspector.cpp
decisions:
  - "Serialize hinge_pivot only when non-default to keep existing scene files clean"
  - "Use const glm::vec3& reference in runtime/editor code so no copies on hot path"
metrics:
  duration: ~8 minutes
  completed: 2026-04-06
  tasks_completed: 3
  files_changed: 7
---

# Phase quick Plan 260406-uqb: Add Editable Hinge Pivot to SingleDoor Summary

Per-door hinge pivot is now data-driven from the .scene file via a `hingePivot` field on `LevelSingleDoorPlacement` and `SingleDoorSpawnSpec`, eliminating the hardcoded `(-0.45, 0, 0.04)` magic vector from editor preview and runtime prefab spawning.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Add hingePivot field to data structs, parser, and serializer | da0a27e | LevelDef.h, LevelDef.cpp, GameplayPrefabData.h, LevelBuilder.cpp |
| 2 | Replace hardcoded offsets in editor preview and runtime prefab, add inspector row | 2b9e111 | GameplayPrefabs.cpp, EditorPreviewWorld.cpp, SingleDoorInspector.cpp |
| 3 | Verify no remaining hardcoded hinge offsets and validate round-trip | (verify only) | — |

## What Was Built

- `LevelSingleDoorPlacement.hingePivot` — `glm::vec3{-0.45f, 0.0f, 0.04f}` default; parsed from `hinge_pivot x y z` token in single_door records
- `SingleDoorSpawnSpec.hingePivot` — same default, populated by LevelBuilder from placement
- Serializer: writes `hinge_pivot` only when value differs from default so existing scene files round-trip cleanly
- `GameplayPrefabs::spawnSingleDoor`: reads `spec.hingePivot` instead of hardcoded local vector
- `EditorPreviewWorld::rebuild()`: reads `placement.hingePivot` for the SingleDoor case
- `EditorPreviewWorld::syncTransforms()`: extracts door payload and reads `door.hingePivot` for leaf positioning
- `SingleDoorInspector`: editable "Hinge Pivot" vec3 row (DragFloat3 via `editVec3`) between Yaw and Open Angle sections

## Verification

- Build: both `pixel-roguelike` and `level-editor` targets compile cleanly
- Grep: no remaining `localHingeOffset(-` or hardcoded `(-0.45f, 0.0f, 0.04f)` calls outside of default initializers and the serializer comparison
- Tests: 29/29 passed (0 failures)

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED

- da0a27e exists: confirmed
- 2b9e111 exists: confirmed
- All 7 modified files contain `hingePivot` references as expected
