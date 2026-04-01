---
phase: quick
plan: 260331-0mj
subsystem: engine-rendering
tags: [mesh-loading, fbx, assimp, multi-material, country-house]
dependency_graph:
  requires: []
  provides: [multi-submesh-fbx-loading]
  affects: [country-house-scene, mesh-library]
tech_stack:
  added: []
  patterns: [submesh-grouping-by-material, named-mesh-registration, asset-cache-keyed-by-material]
key_files:
  created: []
  modified:
    - src/engine/rendering/geometry/MeshGeometry.h
    - src/engine/rendering/assets/AssimpLoader.h
    - src/engine/rendering/assets/AssimpLoader.cpp
    - src/engine/rendering/assets/ModelLoader.h
    - src/engine/rendering/assets/ModelLoader.cpp
    - src/engine/rendering/geometry/MeshLibrary.h
    - src/engine/rendering/geometry/MeshLibrary.cpp
    - src/game/levels/GameAssets.cpp
    - assets/scenes/country_house.scene
decisions:
  - Use # as separator for baseName#materialName mesh IDs — not valid in file paths, not used in FBX material names, visually clear in scene files
  - Group by material index, not by individual submesh — 5 meshes instead of 22, matches intent of per-material rendering
  - Eager loading via loadFromFileMulti rather than lazy alias — simplest and sufficient for the country_house use case
  - Per-material suffix on cache key (filepath + # + materialName) avoids collision with existing single-mesh cache entries
metrics:
  duration: 35 minutes
  completed: 2026-03-31
  tasks: 2
  files: 9
---

# Quick Task 260331-0mj: Multi-Submesh FBX Loading Summary

**One-liner:** Parallel `loadRawMulti/loadFromFileMulti` API groups FBX submeshes by material into `baseName#materialName` meshes, enabling country_house.fbx to render 5 independently-materialed groups instead of one merged mesh.

## What Was Built

Added a parallel multi-submesh loading path alongside the existing single-mesh API. The new path groups submeshes by Assimp material index, merges each group into a single RawMeshData, and registers them as `baseName#materialName` keys in MeshLibrary.

The country_house.scene was updated from one merged mesh to five separate mesh entries, one per material group, each referencing a distinct material ID.

## Tasks Completed

### Task 1: Add multi-mesh loading API to engine layer (commit 95a2385)

Added `NamedRawMeshData` struct to `MeshGeometry.h`, `loadRawMulti` to `AssimpLoader` and `ModelLoader`, and `loadFromFileMulti` to `MeshLibrary`.

Key implementation details:
- `loadGroupedByMaterial()` in AssimpLoader.cpp builds a `unordered_map<unsigned, vector<unsigned>>` keyed by material index, then merges each group using the same vertex/normal/uv/tangent extraction as `loadMergedRaw`
- Results are sorted by material index for deterministic ordering
- Empty material names fall back to `"material_N"` where N is the index
- `MeshLibrary::loadFromFileMulti` uses cache keys of the form `resolvedPath#materialName` to avoid collision with existing single-mesh cache entries

### Task 2: Wire country_house scene (commit 5a3a32d)

Changed `GameAssets.cpp` from `registerFileAlias("country_house", ...)` to `loadFromFileMulti("country_house", ...)`. Updated `country_house.scene` to reference 5 submesh groups:

- `country_house#OldHouseMapWood02` → `ch_wood_exterior`
- `country_house#OldHouseMapIndoorWall01` → `ch_indoor_wall`
- `country_house#OldHouseMapWood01` → `ch_wood_struct`
- `country_house#WindowsMat01` → `ch_window_frame`
- `country_house#q11:Glass` → `ch_glass`

## Decisions Made

- `#` separator: safe in all mesh IDs (not a valid filesystem character, not used in FBX material names)
- Grouping by material rather than individual submesh: 5 groups instead of 22, better UX in editor
- Eager loading in GameAssets: simpler than a lazy alias and loads immediately at startup
- Cache key includes material suffix: no collision with existing cache entries

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None. All 5 material groups are fully wired to distinct material IDs. The material IDs (`ch_indoor_wall`, `ch_wood_struct`, `ch_window_frame`, `ch_glass`) reference existing `.material` files in `assets/materials/`.

## Self-Check: PASSED

- MeshGeometry.h: FOUND
- AssimpLoader.cpp: FOUND
- MeshLibrary.cpp: FOUND
- country_house.scene: FOUND
- GameAssets.cpp: FOUND
- Commit 95a2385: FOUND
- Commit 5a3a32d: FOUND
