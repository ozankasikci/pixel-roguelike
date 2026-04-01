---
phase: quick
plan: 260401-uvc
subsystem: engine/rendering/assets
tags: [fbx, assimp, mesh-import, scale, unit-conversion]
dependency_graph:
  requires: []
  provides: [fbx-unit-conversion]
  affects: [AssimpLoader, country_house.scene, GenericFileScene]
tech_stack:
  added: []
  patterns: [AI_CONFIG_FBX_CONVERT_TO_M via SetPropertyBool before ReadFile]
key_files:
  created: []
  modified:
    - src/engine/rendering/assets/AssimpLoader.cpp
    - assets/scenes/country_house.scene
    - src/game/scenes/GenericFileScene.cpp
key_decisions:
  - "AI_CONFIG_FBX_CONVERT_TO_M set via SetPropertyBool on both loadRaw() and loadRawMulti() Assimp::Importer instances — converts FBX cm vertices to meters at import time, no manual 0.01 scale needed"
  - "FBX mesh cache entries (country_house*, wood_door*) manually cleared after import settings change — cache keys hash source file content not importer settings, so manual clear is required"
metrics:
  duration_minutes: 15
  completed_date: "2026-04-01"
  tasks_completed: 3
  files_changed: 3
---

# Quick Task 260401-uvc: FBX cm-to-m Auto-Conversion via AI_CONFIG_FBX_CONVERT_TO_M Summary

**One-liner:** Enabled Assimp's built-in FBX centimeter-to-meter conversion in both AssimpLoader import paths and removed all manual 0.01 scale workarounds from scene files and scripted geometry.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Enable FBX cm-to-m conversion in AssimpLoader and clear stale cache | fd271a6 | `src/engine/rendering/assets/AssimpLoader.cpp` |
| 2 | Remove 0.01 scale workarounds from scene files and scripted geometry | 5d0b37e | `assets/scenes/country_house.scene`, `src/game/scenes/GenericFileScene.cpp` |
| 3 | Build and verify meshes render at correct scale | (no files) | Build verification only |

## What Changed

### AssimpLoader.cpp

Added `#include <assimp/config.h>` and `importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true)` before each `ReadFile()` call in both `loadRaw()` and `loadRawMulti()`. This flag instructs Assimp's FBX importer to read the file's `UnitScaleFactor` metadata and convert vertex positions to meters during import. glTF imports go through `GltfLoader`, not `AssimpLoader`, so they are unaffected.

### country_house.scene

All 8 mesh lines changed from `0.01 0.01 0.01` scale to `1.0 1.0 1.0` scale. Updated comment to document that conversion is now automatic.

### GenericFileScene.cpp

Both `wood_door` addMesh calls in `buildInstitutionalRoomGeometry()` changed from `glm::vec3(0.01f, 0.01f, 0.01f)` to `glm::vec3(1.0f, 1.0f, 1.0f)`.

### Cache Invalidation

Manually deleted all FBX-sourced cached mesh binaries (`country_house*`, `wood_door*`) from `.cache/meshes/`. The cache hashes source file content, not importer settings, so it would not auto-invalidate after changing the import flag. glTF caches (`arch`, `pillar`, `hand_low_poly`, `gothic_door_static`) were left intact.

## Verification

1. `grep -c "AI_CONFIG_FBX_CONVERT_TO_M" src/engine/rendering/assets/AssimpLoader.cpp` returns 2
2. `grep -c "0.01 0.01 0.01" assets/scenes/country_house.scene` returns 0
3. `grep -c "0.01f, 0.01f, 0.01f" src/game/scenes/GenericFileScene.cpp` returns 0
4. Project builds without errors (both `pixel-roguelike` and `level-editor`)
5. No FBX mesh cache files in `.cache/meshes/` (country_house*, wood_door*)

All checks passed.

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None introduced by this plan.

## Self-Check: PASSED

- `src/engine/rendering/assets/AssimpLoader.cpp` — modified and committed (fd271a6)
- `assets/scenes/country_house.scene` — modified and committed (5d0b37e)
- `src/game/scenes/GenericFileScene.cpp` — modified and committed (5d0b37e)
- Both commits verified present in git log
