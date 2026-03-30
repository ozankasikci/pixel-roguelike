# Quick Task: Multi-Submesh FBX Loading - Research

**Researched:** 2026-03-30
**Domain:** Assimp multi-mesh loading, MeshLibrary registration
**Confidence:** HIGH

## Summary

The current `AssimpLoader::loadRaw()` merges all submeshes from an FBX file into a single `RawMeshData` via `loadMergedRaw()`. This discards per-mesh material assignments, making it impossible to render different parts of a model with different materials. The country_house.fbx has 22 submeshes across 5 materials -- currently all rendered with a single material.

The fix requires: (1) a new `AssimpLoader::loadRawMulti()` that returns per-submesh data with material names, (2) `MeshLibrary::loadFromFileMulti()` that registers each submesh as `basename#submeshname`, (3) updating the country_house.scene to reference individual submeshes.

**Primary recommendation:** Add parallel multi-mesh API alongside existing single-mesh API. Do not change existing signatures -- backward compatibility is critical since glTF and procedural paths must continue working unchanged.

## Current Loading Path Analysis

### AssimpLoader (engine layer)

**Import flags** (line 16-27 of AssimpLoader.cpp):
```
aiProcess_Triangulate
aiProcess_JoinIdenticalVertices
aiProcess_ImproveCacheLocality
aiProcess_RemoveRedundantMaterials
aiProcess_FindInvalidData
aiProcess_GenUVCoords
aiProcess_TransformUVCoords
aiProcess_GenSmoothNormals
aiProcess_CalcTangentSpace
aiProcess_PreTransformVertices   <-- flattens hierarchy, keeps submeshes separate
aiProcess_SortByPType
```

**Key observation:** `aiProcess_PreTransformVertices` flattens the node hierarchy and bakes transforms into vertex positions. After this flag, all `aiMesh` objects in `scene->mMeshes` are at world-space positions. The meshes are NOT merged -- they remain separate `aiMesh` entries. The current code explicitly merges them in `loadMergedRaw()`.

**Data flow:**
```
AssimpLoader::loadRaw(filepath)
  -> Assimp::Importer::ReadFile(filepath, kAssimpImportFlags)
  -> loadMergedRaw(*scene)  // merges ALL submeshes, discards material info
  -> returns single RawMeshData
```

### MeshLibrary (engine layer)

- `registerFileAlias(name, filepath)` -- deferred loading, loads on first `get()`
- `loadFromFile(name, filepath)` -- immediate load via `ModelLoader::loadRaw()` -> single mesh
- `loadFromFile` also writes to `AssetCache` for disk caching

### ModelLoader (engine layer)

- `loadRaw(filepath)` dispatches to `GltfLoader::loadRaw()` or `AssimpLoader::loadRaw()` by extension
- Returns single `RawMeshData`

### Scene files

Scene `.scene` files reference mesh IDs by name: `mesh country_house 0.0 0.0 0.0 ...`
Each mesh entity gets one `materialId` string. No concept of sub-materials per mesh.

## Assimp API for Multi-Mesh + Material Access

**Confidence: HIGH** (verified against installed headers v6.0.4)

### Per-mesh data available from aiScene:
- `scene->mNumMeshes` -- count of mesh primitives (22 for country_house.fbx)
- `scene->mMeshes[i]->mName.C_Str()` -- mesh name (e.g., "q10:Mesh", "hallway", "floor")
- `scene->mMeshes[i]->mMaterialIndex` -- index into `scene->mMaterials[]`
- `scene->mMaterials[idx]->GetName()` returns `aiString` with the material name

### Material names in country_house.fbx:

| Index | Material Name | Used By Meshes |
|-------|--------------|----------------|
| 0 | OldHouseMapWood02 | q10:Mesh(0), floor(15), door_frames(16), windowsCeiling(18), Window(19) |
| 1 | OldHouseMapIndoorWall01 | q9:Mesh1(1), Room08(3), Room01(4), Room02(5), Room09(7), hallway(8), polySurface1-5(9-13), plitus2(14), heating_stove(17) |
| 2 | OldHouseMapWood01 | mainHouse01(2), Room05(6) |
| 3 | WindowsMat01 | Windows(20) |
| 4 | q11:Glass | Windows01Glass(21) |

### Mesh names in country_house.fbx (22 meshes):

| Idx | Mesh Name | Verts | Material |
|-----|-----------|-------|----------|
| 0 | q10:Mesh | 970 | OldHouseMapWood02 |
| 1 | q9:Mesh1 | 466 | OldHouseMapIndoorWall01 |
| 2 | mainHouse01 | 280 | OldHouseMapWood01 |
| 3-8 | Room08, Room01, Room02, Room05, Room09, hallway | 38-88 | Mixed |
| 9-13 | polySurface1-5 | 4-6 | OldHouseMapIndoorWall01 |
| 14 | plitus2 | 128 | OldHouseMapIndoorWall01 |
| 15 | floor | 83 | OldHouseMapWood02 |
| 16 | door_frames | 565 | OldHouseMapWood02 |
| 17 | heating_stove | 204 | OldHouseMapIndoorWall01 |
| 18 | windowsCeiling | 875 | OldHouseMapWood02 |
| 19 | Window | 222 | OldHouseMapWood02 |
| 20 | Windows | 2282 | WindowsMat01 |
| 21 | Windows01Glass | 64 | q11:Glass |

## Recommended Approach

### Strategy: Group by material, not by submesh

Rather than registering 22 individual submeshes (most are tiny), **merge submeshes that share the same material** into a single mesh. This produces 5 meshes from the country_house.fbx, one per material:

- `country_house#OldHouseMapWood02` -- exterior wood + floor + door frames + windows ceiling
- `country_house#OldHouseMapIndoorWall01` -- interior walls + rooms + hallway + stove
- `country_house#OldHouseMapWood01` -- main house structure exterior
- `country_house#WindowsMat01` -- window frames
- `country_house#q11:Glass` -- glass panes

This is a better user experience (5 scene entities instead of 22) and matches the intent (apply different materials to different material groups).

### New types and functions needed:

**1. New struct in MeshGeometry.h:**
```cpp
struct NamedRawMeshData {
    std::string name;          // material name from FBX
    RawMeshData mesh;
};
```

**2. New static method in AssimpLoader:**
```cpp
// Returns one RawMeshData per unique material, merged from all submeshes
// sharing that material. Name is the Assimp material name.
static std::vector<NamedRawMeshData> loadRawMulti(const std::string& filepath);
```

Implementation: reuse the same `kAssimpImportFlags`. Instead of `loadMergedRaw()`, iterate `scene->mMeshes`, group by `mMaterialIndex`, merge each group, and tag with the material name from `scene->mMaterials[idx]->GetName()`.

**3. New static method in ModelLoader:**
```cpp
static std::vector<NamedRawMeshData> loadRawMulti(const std::string& filepath);
```
Dispatches to `AssimpLoader::loadRawMulti()` for .fbx; for .glb/.gltf returns single-element vector with the merged mesh (no material info from glTF loader currently).

**4. New method in MeshLibrary:**
```cpp
void loadFromFileMulti(const std::string& baseName, const std::string& filepath);
```
Calls `ModelLoader::loadRawMulti()`, registers each result as `baseName#materialName`. Also writes each to AssetCache with a cache key that includes the material name suffix.

**5. Scene file changes (country_house.scene):**
Replace the single `mesh country_house ...` line with 5 lines:
```
mesh country_house#OldHouseMapWood02 0.0 0.0 0.0 0.01 0.01 0.01 ... material ch_wood_exterior
mesh country_house#OldHouseMapIndoorWall01 0.0 0.0 0.0 0.01 0.01 0.01 ... material ch_indoor_wall
mesh country_house#OldHouseMapWood01 0.0 0.0 0.0 0.01 0.01 0.01 ... material ch_wood_struct
mesh country_house#WindowsMat01 0.0 0.0 0.0 0.01 0.01 0.01 ... material ch_window_frame
mesh country_house#q11:Glass 0.0 0.0 0.0 0.01 0.01 0.01 ... material ch_glass
```

**6. Asset registration:**
In `GameAssets.cpp`, replace `registerFileAlias("country_house", ...)` with a call that triggers multi-mesh loading. Two options:
- Option A: Add `registerFileAliasMulti(baseName, filepath)` to MeshLibrary
- Option B: Eagerly call `loadFromFileMulti("country_house", "assets/meshes/country_house.fbx")` in GameAssets

Option A (lazy alias) is cleaner but more complex. Option B is simpler and sufficient for now.

## aiProcess_PreTransformVertices Safety

**Confidence: HIGH**

`aiProcess_PreTransformVertices` flattens node transforms into vertex positions but does NOT merge meshes. Each `aiMesh` in the scene remains separate with its own `mMaterialIndex`. This is exactly what we need -- vertex positions are already in world space (or rather, in the root coordinate system of the FBX), and each mesh retains its material assignment.

The flag is safe and desirable for this use case. Without it, we'd need to walk the node hierarchy and multiply transforms manually.

## Edge Cases

### Material name sanitization
- Material names can contain colons (e.g., `q11:Glass`). The `#` separator in `basename#materialname` is safe because `#` does not appear in FBX material names or file stems.
- Verify: `MeshLibrary` stores names as `std::string` keys in `unordered_map` -- no restrictions on characters.

### Empty/missing material names
- If `aiMaterial::GetName()` returns empty string, use `"material_N"` where N is the material index.
- All 5 materials in country_house.fbx have names, so this is a defensive measure only.

### Single-mesh files
- `loadRawMulti()` for an FBX with one material should return a single-element vector. The `#` naming still works: `door#DoorMaterial`. But for backward compat, if there's only one material group, could register both `basename` and `basename#matname`.

### Backward compatibility
- Existing `loadRaw()` and `load()` remain unchanged. All glTF and procedural mesh paths are unaffected.
- Existing scene files referencing `country_house` (merged) continue to work until the scene is updated.
- The `country_house_door.fbx` and `country_house_doors.fbx` likely have 1-2 materials each and may not need multi-mesh treatment immediately.

### Asset cache invalidation
- Current cache key is based on file path hash. Multi-mesh loading needs per-material cache entries.
- Use cache key: `filepath + "#" + materialName` to avoid collision with the single-mesh cache entry.
- The old single-mesh cache entry for `country_house.fbx` becomes stale but harmless (never looked up once code switches to multi-mesh loading).

### Naming convention: `#` as separator
- `#` is chosen because: (1) not valid in file paths on any OS, (2) not used in Assimp material names, (3) visually clear in scene files and logs.
- The scene file parser already handles arbitrary mesh ID strings -- no changes needed to the `.scene` format.

## Files That Need Changes

| File | Change | Scope |
|------|--------|-------|
| `src/engine/rendering/geometry/MeshGeometry.h` | Add `NamedRawMeshData` struct | 4 lines |
| `src/engine/rendering/assets/AssimpLoader.h` | Add `loadRawMulti()` declaration | 1 line |
| `src/engine/rendering/assets/AssimpLoader.cpp` | Add `loadRawMulti()` implementation | ~50 lines |
| `src/engine/rendering/assets/ModelLoader.h` | Add `loadRawMulti()` declaration | 1 line |
| `src/engine/rendering/assets/ModelLoader.cpp` | Add `loadRawMulti()` implementation | ~15 lines |
| `src/engine/rendering/geometry/MeshLibrary.h` | Add `loadFromFileMulti()` declaration | 1 line |
| `src/engine/rendering/geometry/MeshLibrary.cpp` | Add `loadFromFileMulti()` implementation | ~40 lines |
| `src/game/levels/GameAssets.cpp` | Replace single alias with multi-mesh load | 5 lines |
| `assets/scenes/country_house.scene` | Split single mesh into 5 per-material meshes | ~10 lines |

**Total estimated diff:** ~130 lines added, ~5 lines removed.

## Common Pitfalls

### Pitfall 1: aiProcess_RemoveRedundantMaterials removes unused materials
The import flags include `aiProcess_RemoveRedundantMaterials`. If two materials have identical properties (but different names), Assimp merges them. This changes material indices. Always read the material index AFTER import, not before. In practice this is fine since we read from the post-processed `aiScene`.

### Pitfall 2: Cache key collision
If `loadFromFile("country_house", path)` and `loadFromFileMulti("country_house", path)` are both called, the single-mesh cache entry and multi-mesh cache entries use different keys (with/without `#suffix`), so no collision. But avoid calling both -- pick one path per file.

### Pitfall 3: The `#` in mesh IDs vs scene file parsing
Verify the scene file tokenizer handles `#` in mesh IDs. The tokenizer uses whitespace-delimited tokens, so `country_house#OldHouseMapWood02` is a single token. No issue expected, but worth a quick check of the parser.

## Sources

### Primary (HIGH confidence)
- Assimp 6.0.4 installed headers (`/opt/homebrew/opt/assimp/include/assimp/`)
  - `mesh.h`: `mName` (line 793), `mMaterialIndex` (line 779)
  - `material.h`: `GetName()` (line 750), `AI_MATKEY_NAME` (line 984)
- Direct inspection of `country_house.fbx` via `assimp info` and `assimp dump` -- 22 meshes, 5 materials, material-to-mesh mapping verified
- Source code analysis of AssimpLoader.cpp, MeshLibrary.cpp, ModelLoader.cpp, LevelLoader.cpp, GameAssets.cpp
