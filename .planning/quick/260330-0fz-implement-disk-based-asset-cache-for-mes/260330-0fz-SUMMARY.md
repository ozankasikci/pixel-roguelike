---
phase: quick
plan: 260330-0fz
subsystem: engine/rendering
tags: [asset-cache, performance, disk-io, mesh-loading, procedural-textures]
dependency_graph:
  requires: []
  provides: [AssetCache, CachedMeshData, CachedTextureData, pre-interleaved-mesh-constructor]
  affects: [MeshLibrary, MaterialTextureLibrary]
tech_stack:
  added: []
  patterns: [FNV-1a-hashing, binary-cache-format, packed-struct-headers]
key_files:
  created:
    - src/engine/rendering/assets/AssetCache.h
    - src/engine/rendering/assets/AssetCache.cpp
    - tests/engine/test_asset_cache.cpp
  modified:
    - .gitignore
    - src/engine/CMakeLists.txt
    - src/engine/rendering/geometry/Mesh.h
    - src/engine/rendering/geometry/Mesh.cpp
    - src/engine/rendering/geometry/MeshLibrary.cpp
    - src/game/rendering/MaterialTextureLibrary.h
    - src/game/rendering/MaterialTextureLibrary.cpp
    - tests/engine/CMakeLists.txt
decisions:
  - "Cache root uses project root detection (walk up to find assets/ dir) for portability"
  - "Mesh cache key is source file content hash (FNV-1a of file bytes) embedded in filename"
  - "Texture cache key is texture key string hashed with FNV-1a, stored as param_hash in header"
  - "File-based textures are NOT cached (per design doc: diminishing returns)"
  - "generateBrickPixels/generateStonePixels refactored out of buildBrickSet/buildStoneSet for single-pass generate+cache"
metrics:
  duration_seconds: 1284
  completed: "2026-03-30T00:45:00Z"
  tasks_completed: 2
  tasks_total: 2
  files_created: 3
  files_modified: 8
---

# Quick Task 260330-0fz: Implement Disk-Based Asset Cache Summary

Disk-based asset cache with FNV-1a content hashing for meshes and procedural textures, eliminating Assimp/tinygltf parsing and procedural generation on subsequent launches.

## Task Results

| Task | Name | Commit | Status |
|------|------|--------|--------|
| 1 | Create AssetCache class and pre-interleaved Mesh constructor | 9c6564c | Done |
| 2 | Integrate AssetCache into MeshLibrary and MaterialTextureLibrary | a2b1c1e | Done |

## What Was Built

### AssetCache (stateless utility class)
- `findMeshCache` / `writeMeshCache` -- binary .mesh.bin files with 48-byte packed header (magic, version, content hash, vertex/index counts, AABB)
- `findTextureCache` / `writeTextureCache` -- binary .tex.bin files with 24-byte packed header (magic, version, width/height, channels, param hash)
- `hashFileContents` / `hashBytes` -- FNV-1a 64-bit hash implementation
- Cache files stored in `.cache/meshes/` and `.cache/textures/` under the project root
- Cache filename encodes the content hash, so modifying a source file creates a different filename (automatic invalidation)

### Pre-interleaved Mesh Constructor
- New `Mesh(interleavedVertices, indices, aabbMin, aabbMax)` constructor that skips interleaving and AABB computation
- Directly uploads pre-interleaved data to GPU -- identical vertex attribute layout (stride 11 floats, locations 0-3)

### MeshLibrary Integration
- `loadFromFile` now checks `AssetCache::findMeshCache` before calling `ModelLoader::loadRaw`
- On cache miss: loads via ModelLoader, builds interleaved data, writes cache, registers mesh
- On cache hit: constructs Mesh directly from cached pre-interleaved data

### MaterialTextureLibrary Integration
- `ensureTextureSet` checks disk cache for all 4 procedural texture maps (albedo, normal, roughness, ao) before generating
- Refactored `buildBrickSet`/`buildStoneSet` into `generateBrickPixels`/`generateStonePixels` returning `ProceduralPixelData`
- On cache miss: generates pixels once, creates GL textures AND writes to disk cache in single pass (no double generation)
- File-based textures (loaded from image files) are NOT cached per design doc

## Deviations from Plan

None -- plan executed exactly as written.

## Known Stubs

None -- all functionality is fully wired.

## Verification

- Full project builds cleanly with no warnings from new files
- `test_asset_cache` passes all 4 test cases: FNV-1a hash correctness, mesh round-trip, mesh invalidation, texture round-trip (including R8 and param hash mismatch)
- `.cache/` is in `.gitignore`
