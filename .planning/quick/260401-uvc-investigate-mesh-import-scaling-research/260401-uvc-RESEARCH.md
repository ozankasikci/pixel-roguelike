# Mesh Import Scaling - Research

**Researched:** 2026-04-01
**Domain:** FBX/glTF import unit conversion in custom engine
**Confidence:** HIGH

## Summary

Imported FBX meshes appear oversized because the engine's AssimpLoader reads vertex positions verbatim without converting units. FBX files default to centimeters (UnitScaleFactor=1 means 1 unit = 1 cm), but the engine uses meters as its world unit. This 100x mismatch means a 2-meter door loads as 200 engine units (200 meters).

The current workaround is manually specifying `scale = (0.01, 0.01, 0.01)` in scene files and scripted geometry, as seen in `country_house.scene` and `buildInstitutionalRoomGeometry()`. This is fragile, error-prone, and forces every FBX placement to know its source unit system.

**Primary recommendation:** Add `AI_CONFIG_FBX_CONVERT_TO_M` to AssimpLoader via `importer.SetPropertyBool()`. This is Assimp's built-in FBX-to-meter conversion and eliminates the need for manual 0.01 scaling.

## Root Cause Analysis

### The Problem

1. **FBX convention**: FBX files use centimeters as their base unit. `UnitScaleFactor=1.0` means "1 file unit = 1 centimeter." Both `wood_door.fbx` and `country_house.fbx` have `UnitScaleFactor=1` and `OriginalUnitScaleFactor=1` -- confirmed by reading the file headers.

2. **Engine convention**: The engine uses 1 unit = 1 meter. Procedural meshes (prison_wall, prison_floor, cube, cylinder) are all authored in meters. Scene positions use meter coordinates (ceiling at Y=4.0, walls at 2m spacing).

3. **AssimpLoader gap**: `AssimpLoader::loadRaw()` creates an `Assimp::Importer`, calls `ReadFile()` with post-processing flags, and copies vertex positions verbatim. No unit conversion is applied. The import flags (`kAssimpImportFlags`) do not include `aiProcess_GlobalScale`. No importer properties are set.

4. **GltfLoader is fine**: glTF spec mandates 1 unit = 1 meter. Our glTF files (arch.glb, pillar.glb) are already in meters. No conversion needed.

### Evidence

| File | Format | Vertex Extent (raw) | Real-world Size | Appears As |
|------|--------|---------------------|-----------------|------------|
| wood_door.fbx | FBX ASCII | 65 x 136 x 2.3 | ~0.65m x 1.36m door | 65m x 136m |
| country_house.fbx | FBX Binary | UnitScaleFactor=1 (cm) | ~10m house | ~1000m structure |
| arch.glb | glTF Binary | 6.4 x 1.3 x 0.9 | 6.4m wide arch | 6.4m (correct) |
| pillar.glb | glTF Binary | 1.4 x 6.0 x 1.4 | 6m tall pillar | 6m (correct) |

### Current Workaround

Scene files manually apply 0.01 scale to FBX meshes:
```
# country_house.scene -- every FBX mesh uses 0.01 scale
mesh country_house#OldHouseMapWood02 0.0 0.0 0.0 0.01 0.01 0.01 0.0 0.0 0.0 ...
```

```cpp
// GenericFileScene.cpp -- scripted geometry also uses 0.01 scale
auto metalDoor = builder.addMesh("wood_door",
    glm::vec3(0.0f, 0.0f, 5.95f),
    glm::vec3(0.01f, 0.01f, 0.01f),  // manual cm->m conversion
    ...);
```

## How Major Engines Handle This

### Unity
- Default "Scale Factor" of 0.01 applied to all FBX imports (effectively cm-to-m)
- `useFileScale: 1` reads the FBX UnitScaleFactor metadata
- Combined: FileScale * ScaleFactor = conversion to Unity meters
- Bakes scale into import, not per-instance transform

### Unreal Engine
- Internal unit = 1 cm (matches FBX default)
- No conversion needed for most FBX files
- Has "Import Uniform Scale" option for manual override

### Godot
- Uses Assimp's `AI_CONFIG_FBX_CONVERT_TO_M` flag
- Applies automatic cm-to-m conversion at import time
- Vertex positions are converted before mesh creation

## Recommended Fix

### Approach: Use Assimp's Built-in FBX Unit Conversion

Assimp provides `AI_CONFIG_FBX_CONVERT_TO_M` (available since our Assimp 6.0.2) which instructs the FBX importer to automatically convert centimeter values to meters during import. This is the cleanest solution because:

1. Conversion happens at the vertex level during import -- no 0.01 scale in transforms
2. Normals, tangents, and other vectors are unaffected (only positions are scaled)
3. No changes needed to GltfLoader (already in meters)
4. No changes needed to MeshLibrary, ModelLoader, or scene format

### Implementation (AssimpLoader.cpp)

```cpp
#include <assimp/config.h>  // for AI_CONFIG_FBX_CONVERT_TO_M

RawMeshData AssimpLoader::loadRaw(const std::string& filepath) {
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);  // ADD THIS
    const aiScene* scene = importer.ReadFile(filepath, kAssimpImportFlags);
    // ... rest unchanged
}

std::vector<NamedRawMeshData> AssimpLoader::loadRawMulti(const std::string& filepath) {
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);  // ADD THIS
    const aiScene* scene = importer.ReadFile(filepath, kAssimpImportFlags);
    // ... rest unchanged
}
```

### Required Follow-up: Fix Existing Scale Overrides

After enabling automatic conversion, all existing manual 0.01 scales must be changed to 1.0:

1. **`assets/scenes/country_house.scene`** -- change all `0.01 0.01 0.01` scale values to `1.0 1.0 1.0`
2. **`src/game/scenes/GenericFileScene.cpp`** -- change wood_door addMesh calls from `glm::vec3(0.01f)` to `glm::vec3(1.0f)`
3. **Any other scene files** referencing FBX-loaded meshes with 0.01 scale

### Asset Cache Invalidation

The AssetCache stores pre-processed mesh data. After changing the import scale, cached mesh binaries will contain the old (unscaled) data. Either:
- Clear the cache directory (simplest)
- The cache should auto-invalidate since it hashes source file contents -- but the source file hasn't changed, only the import settings. **Manual cache clear is required.**

## Common Pitfalls

### Pitfall 1: FBX Files with Non-Standard UnitScaleFactor
**What goes wrong:** Some FBX exporters write UnitScaleFactor=100 (meaning "1 file unit = 1 meter" in FBX parlance). Applying cm-to-m conversion to these would shrink them 100x.
**How to avoid:** `AI_CONFIG_FBX_CONVERT_TO_M` reads the file's UnitScaleFactor metadata and applies the correct conversion. It does not blindly divide by 100.
**Confidence:** MEDIUM -- verify by testing with files that have different UnitScaleFactor values.

### Pitfall 2: Blender FBX Exports
**What goes wrong:** Blender's FBX exporter can write UnitScaleFactor=100 or apply its own scaling depending on export settings. This can interact unpredictably with the engine's conversion.
**How to avoid:** Test with Blender exports. If issues persist, use glTF (`.glb`) export from Blender instead -- glTF always uses meters.

### Pitfall 3: Mixed FBX/glTF Scenes
**What goes wrong:** After fixing FBX import, new FBX meshes appear at correct scale while glTF meshes are unchanged. No issue here since both will be in meters.
**How to avoid:** This is actually the correct outcome. Both formats normalize to meters.

### Pitfall 4: Cache Staleness
**What goes wrong:** Old cached mesh data persists with pre-conversion coordinates.
**How to avoid:** Clear `.cache/` or asset cache directory after changing import settings.

## Alternative Approaches (Not Recommended)

| Approach | Why Not |
|----------|---------|
| Manually divide positions by 100 in AssimpLoader | Fragile; doesn't handle UnitScaleFactor variations; reinvents what Assimp already provides |
| Keep 0.01 scale in scene files | Error-prone; forces authors to know source format; breaks editor add-mesh workflow |
| Use `aiProcess_GlobalScale` + `AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY` | More general but requires manual scale factor; `AI_CONFIG_FBX_CONVERT_TO_M` is the purpose-built solution |
| Convert all FBX to glTF before import | Extra build step; loses FBX-specific features like multi-material groups |

## Sources

### Primary (HIGH confidence)
- Assimp 6.0.2 installed headers (`/opt/homebrew/Cellar/assimp/6.0.2/include/assimp/config.h`) -- confirmed `AI_CONFIG_FBX_CONVERT_TO_M` and `AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY` exist
- Assimp postprocess.h -- confirmed `aiProcess_GlobalScale` flag definition
- Direct FBX file inspection -- `wood_door.fbx` ASCII header shows `UnitScaleFactor=1`, `OriginalUnitScaleFactor=1`
- Codebase inspection -- AssimpLoader.cpp, GltfLoader.cpp, MeshLibrary.cpp, GenericFileScene.cpp

### Secondary (MEDIUM confidence)
- [Assimp discussion on unit scale handling](https://github.com/orgs/assimp/discussions/4018)
- [Assimp FBX unit scale issue](https://github.com/assimp/assimp/issues/2166)
- [Unity FBX import documentation](https://docs.unity3d.com/Manual/FBXImporter-Model.html)
- [Godot Assimp FBX import integration](https://github.com/godotengine/godot/commit/ad214c03560d721d9b8bbff03835fc7fa4884943)

## Metadata

**Confidence breakdown:**
- Root cause diagnosis: HIGH -- confirmed by reading raw vertex data and FBX metadata
- Recommended fix: HIGH -- `AI_CONFIG_FBX_CONVERT_TO_M` exists in installed Assimp headers
- Edge cases (non-standard UnitScaleFactor): MEDIUM -- needs testing with varied FBX files

**Research date:** 2026-04-01
**Valid until:** Stable (Assimp API is mature; FBX format is frozen)
