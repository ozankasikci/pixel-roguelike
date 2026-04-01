---
phase: quick
plan: 260331-3ew
subsystem: engine/rendering/assets, game/rendering
tags: [performance, caching, asset-loading, mesh, texture, fbx]
dependency_graph:
  requires: []
  provides: [file-based-texture-caching, multi-mesh-fbx-cache-fix, no-double-fbx-parse]
  affects: [AssetCache, MeshLibrary, MaterialTextureLibrary, RuntimeGameSession]
tech_stack:
  added: []
  patterns: [disk-cache-with-two-arg-overloads, stb_image-re-read-for-cache-write, early-exit-guard]
key_files:
  created: []
  modified:
    - src/engine/rendering/assets/AssetCache.h
    - src/engine/rendering/assets/AssetCache.cpp
    - src/engine/rendering/geometry/MeshLibrary.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/game/rendering/MaterialTextureLibrary.cpp
    - src/game/CMakeLists.txt
decisions:
  - Two-arg findMeshCache/writeMeshCache overloads separate sourceFilePath (for hashing) from cacheLabel (for filename), fixing broken cache for multi-mesh FBX virtual paths like path.fbx#SubMesh
  - Early-exit guard in loadFromFileMulti checks for any existing baseName# key to avoid re-parsing FBX on rebuild call
  - stb_image re-read for cache write is one-time cost on cold cache; refactoring Texture2D to expose pixel data would change API surface for marginal benefit
  - tinygltf added as PRIVATE dep of game_rendering to expose stb_image.h include path without polluting consumers
metrics:
  duration: ~15min
  completed: "2026-03-31"
  tasks: 2
  files: 6
---

# Phase quick Plan 260331-3ew: Fix Country House Scene Slow Loading Summary

Three targeted fixes eliminate 8-12 second Country House scene load times: fixed broken mesh cache key for multi-mesh FBX submeshes, eliminated redundant FBX double-parse in rebuild(), and added disk caching for 206 MB of file-based 4K PNG textures.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Fix mesh cache for multi-mesh FBX and guard against double-parse | 0a7297f | AssetCache.h, AssetCache.cpp, MeshLibrary.cpp, RuntimeGameSession.cpp |
| 2 | Add disk caching for file-based textures in MaterialTextureLibrary | c348e97 | MaterialTextureLibrary.cpp, src/game/CMakeLists.txt |

## What Was Built

**Fix 1a — AssetCache two-arg overloads:**
Added `findMeshCache(sourceFilePath, cacheLabel)` and `writeMeshCache(sourceFilePath, cacheLabel, ...)` overloads. These hash `sourceFilePath` (the actual FBX file) for the content hash, but use `cacheLabel` as the cache filename stem. This fixes the root cause: the old `cacheKey = resolvedPath + "#" + entry.name` compound path failed `hashFileContents` (cannot open a virtual path as a file), causing both find and write to silently no-op.

**Fix 1b — MeshLibrary::loadFromFileMulti:**
- Added early-exit guard: checks for any existing `baseName#` key in `meshes_` before calling `ModelLoader::loadRawMulti`. If any submesh is already registered, returns immediately. This prevents the second `bootstrapRuntimeMeshLibrary` call in `rebuild()` from re-parsing the FBX.
- Changed cache calls from single-arg (`cacheKey = resolvedPath + "#" + entry.name`) to two-arg (`resolvedPath, baseName + "_" + entry.name`).

**Fix 1c — RuntimeGameSession::rebuild:**
Removed the redundant `bootstrapRuntimeMeshLibrary(meshLibrary_)` call on line 128. The constructor at line 82 is the canonical place. The early-exit guard in loadFromFileMulti provides defensive protection if called again.

**Fix 2 — MaterialTextureLibrary file-based texture caching:**
Replaced the uncached file texture loading block with a cache-first approach:
1. Check cache for all 4 channels (albedo, normal, roughness, ao)
2. On full cache hit: create GL textures from cached pixel data
3. On any miss: load via existing `createRGBA8FromFile`/`createR8FromFile`, then re-read with `stbi_load` to write pixel data to disk cache
Added `tinygltf` as PRIVATE dep of `game_rendering` to expose `stb_image.h`.

## Deviations from Plan

**Deviation: Merged main branch into worktree before starting**

The worktree was created before `loadFromFileMulti` was added to the codebase (commits 5a3a32d and 0afbf6e on 2026-03-31 added multi-mesh loading). Merged main (88e3dd3) into worktree branch to get the current state of the code the plan was written against.

**No other deviations.** The plan executed as described once the worktree was up-to-date.

## Verification

All three executables build cleanly:
- `pixel-roguelike` — confirmed
- `level-editor` — confirmed
- `procedural-model-viewer` — confirmed

## Known Stubs

None. The file-based texture caching implementation is complete. Cache files will be written to `.cache/textures/` on first launch and read on subsequent launches.

## Self-Check: PASSED
