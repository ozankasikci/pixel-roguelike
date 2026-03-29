# Asset Cache Design

**Date:** 2026-03-30
**Goal:** Persist processed mesh geometry and procedural textures to disk so that reopening the editor (or switching scenes) skips expensive parsing, generation, and interleaving on cache hit.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Cache location | `.cache/` in project root, gitignored | Simple, per-project, easy to nuke |
| What to cache | Processed meshes + procedural textures | Biggest two bottlenecks; file-based textures (PNGs) have diminishing returns |
| Invalidation | Content hash (FNV-1a 64-bit) | Bulletproof across git operations; hashing cost negligible vs parsing cost |
| File format | Raw binary with small header | Zero dependencies, fastest possible read path (fread + glBufferData) |

## Directory Structure

```
.cache/
├── meshes/
│   ├── arch_a7f3bc01.mesh.bin
│   └── door_e2d14f88.mesh.bin
└── textures/
    ├── stone_c9a2b310.tex.bin
    └── brick_f1e07d42.tex.bin
```

## Binary Formats

### Mesh (`.mesh.bin`)

```
[Header - 32 bytes]
  magic:        4 bytes  "MSH\0"
  version:      1 byte   (currently 1)
  padding:      3 bytes
  content_hash: 8 bytes  (FNV-1a of source file)
  vertex_count: 4 bytes  (uint32)
  index_count:  4 bytes  (uint32)
  aabb_min:     12 bytes (3x float)
  aabb_max:     12 bytes (3x float)

[Vertex Data - vertex_count * 44 bytes]
  Interleaved: pos(3f) + normal(3f) + uv(2f) + tangent(3f)

[Index Data - index_count * 4 bytes]
  uint32 indices
```

### Procedural Texture (`.tex.bin`)

```
[Header - 24 bytes]
  magic:        4 bytes  "TEX\0"
  version:      1 byte   (currently 1)
  padding:      1 byte
  width:        2 bytes  (uint16)
  height:       2 bytes  (uint16)
  channels:     1 byte   (4 for RGBA, 1 for R8)
  padding:      1 byte
  param_hash:   8 bytes  (FNV-1a of serialized generation parameters)
  reserved:     4 bytes

[Pixel Data - width * height * channels bytes]
  Raw pixels, ready for glTexImage2D
```

## Integration

### New Class: `AssetCache`

Location: `src/engine/rendering/assets/AssetCache.h` / `.cpp`

Stateless utility. No GL context needed. Provides:
- `findMeshCache(filepath) -> optional<CachedMeshData>`
- `writeMeshCache(filepath, interleaved_verts, indices, aabb)`
- `findTextureCache(param_hash) -> optional<CachedTextureData>`
- `writeTextureCache(param_hash, pixels, w, h, channels)`

### Mesh Path (`MeshLibrary::loadFromFile`)

```
filepath -> AssetCache::findMeshCache(filepath)
  HIT:  read binary -> Mesh(pre-interleaved data, aabb)  [GPU upload only]
  MISS: GltfLoader/AssimpLoader -> Mesh(raw verts/indices) [full path]
        -> AssetCache::writeMeshCache(...)
```

New `Mesh` constructor overload accepts pre-interleaved vertex data + precomputed AABB.

### Procedural Texture Path (`MaterialTextureLibrary::ensureTextureSet`)

```
params -> AssetCache::findTextureCache(param_hash)
  HIT:  read binary -> glTexImage2D + mipmaps
  MISS: buildBrickSet()/buildStoneSet() -> raw pixels
        -> AssetCache::writeTextureCache(...)
        -> glTexImage2D + mipmaps
```

### Cache Layering

```
Request -> In-memory cache (MeshLibrary / MaterialTextureLibrary)
  HIT:  return immediately (scene switching within running session)
  MISS: -> Disk cache (AssetCache)
           HIT:  read binary, populate in-memory cache
           MISS: full load/generate, write disk cache, populate in-memory cache
```

## Hashing

- **Mesh files:** FNV-1a 64-bit over entire source file bytes (~1 GB/s, <5ms for typical assets)
- **Procedural textures:** FNV-1a 64-bit over serialized parameter struct (type enum + floats in fixed order)
- Hash appears in both filename (for filesystem lookup) and header (for validation)
- On read: if header hash != freshly computed hash, treat as miss and regenerate

## Invalidation & Cleanup

- No background GC. Stale files remain until name slot reused or user deletes `.cache/`.
- Optional editor menu action ("Clear Asset Cache") can be added later.
- Bumping header version byte auto-invalidates all caches of that type.
