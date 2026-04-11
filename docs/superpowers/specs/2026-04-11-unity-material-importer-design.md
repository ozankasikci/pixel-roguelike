# Unity Material Importer — Design Spec

## Goal

Add a right-click "Import Material to Project" context menu item in the editor's asset browser for Unity `.mat` files. Converts a Unity Standard shader material into the engine's `.material` format and writes it to `assets/materials/`.

## Scope

- Single-file import via right-click context menu
- GUID-based texture resolution via `.meta` files
- Targets Unity Standard shader `.mat` files from asset packs
- Output immediately usable in the editor (hot-reload into ContentRegistry)

## Conversion Logic

### Input: Unity `.mat` (YAML)

Unity Standard shader materials store:
- Texture references as GUIDs (`{fileID: 2800000, guid: <hex>, type: 3}`)
- Scalar floats (`_Metallic`, `_GlossMapScale`, `_BumpScale`, `_OcclusionStrength`, etc.)
- Colors (`_Color` as RGBA)
- Shader keywords (`_METALLICGLOSSMAP`, `_NORMALMAP`, `_PARALLAXMAP`)
- A null texture reference is `{fileID: 0}` — no texture assigned for that slot

### Texture Resolution

Resolve texture paths via Unity GUID system:

1. Parse the `.mat` file to extract GUIDs from texture slots (`_MainTex`, `_BumpMap`, `_MetallicGlossMap`, `_OcclusionMap`)
2. Skip slots with `fileID: 0` (no texture assigned)
3. Build a GUID-to-path map by scanning all `.png.meta` files under the pack's `Texture/` directory (only ~100 files — trivial)
4. Each `.meta` file has `guid: <hex>` on line 2 — extract it and map to the adjacent `.png` path

This handles edge cases like `Cable_Red.mat` which shares textures from the `Cable/` folder via GUIDs rather than having its own texture set.

### Texture Slot Mapping

| Unity slot | Engine field | Notes |
|---|---|---|
| `_MainTex` | `albedo_map` | Base color / diffuse |
| `_BumpMap` | `normal_map` | Normal map |
| `_OcclusionMap` | `ao_map` | Ambient occlusion |
| `_MetallicGlossMap` | `roughness_map` | Unity packs metallic (R) + smoothness (A) — our shader reads it as roughness |

Slots not mapped: `_ParallaxMap` (engine doesn't support height maps), `_EmissionMap` (engine only has `emissive_strength` scalar).

### Property Mapping

| Unity property | Engine property | Transform |
|---|---|---|
| `_Color` (RGBA) | `base_color` (RGB) | Drop alpha channel |
| `_Metallic` | `metalness` | Direct (0.0-1.0) |
| `_GlossMapScale` | `roughness_bias` | Invert: `1.0 - value` |
| `_BumpScale` | `normal_strength` | Direct |
| `_OcclusionStrength` | `ao_strength` | Direct |

### Defaults

When Unity properties are missing or at their defaults, use these engine defaults:
- `base_color`: 1.0 1.0 1.0
- `uv_mode`: mesh
- `uv_scale`: 1.0 1.0
- `normal_strength`: 1.0
- `roughness_scale`: 1.0
- `roughness_bias`: 0.5
- `metalness`: 0.0
- `ao_strength`: 0.9

### Output Naming

Material name is derived from the `.mat` filename, lowercased, with a `qdp_` prefix:
- `DoorA.mat` -> `qdp_door_a.material`
- `Cable_Red.mat` -> `qdp_cable_red.material`
- `FuseBoxA.mat` -> `qdp_fuse_box_a.material`

The `qdp_` prefix follows the existing convention in `assets/materials/`.

PascalCase to snake_case conversion: insert `_` before each uppercase letter that follows a lowercase letter, then lowercase everything.

### Output Format

```
id qdp_door_a
base_color 1.0 1.0 1.0
uv_mode mesh
uv_scale 1.0 1.0
normal_strength 1.0
roughness_scale 1.0
roughness_bias 0.534
metalness 0.0
ao_strength 1.0
albedo_map assets/packs/QuestDoorsPack/Texture/DoorA/T_DoorA_BaseColor.png
normal_map assets/packs/QuestDoorsPack/Texture/DoorA/T_DoorA_Normal.png
ao_map assets/packs/QuestDoorsPack/Texture/DoorA/T_DoorA_AO.png
roughness_map assets/packs/QuestDoorsPack/Texture/DoorA/T_DoorA_Roughness.png
```

Texture paths are relative to the project root (same convention as existing `.material` files).

### Duplicate Handling

If `assets/materials/{output_name}.material` already exists, skip the import and log a warning. Do not overwrite.

## Architecture

### New Files

**`src/editor/assets/EditorMaterialImporter.h`**
```cpp
#pragma once
#include <filesystem>
#include <string>

struct ImportedMaterialResult {
    bool success = false;
    std::string materialId;        // e.g. "qdp_door_a"
    std::string outputPath;        // e.g. "assets/materials/qdp_door_a.material"
    std::string errorMessage;      // non-empty on failure
};

// Returns true if the file is a Unity .mat file that can be imported.
bool canImportUnityMaterial(const std::filesystem::path& path);

// Convert a Unity .mat file to an engine .material file.
// Writes to assetsRoot/materials/{derived_name}.material.
// packRoot is the pack directory containing both Material/ and Texture/ folders.
ImportedMaterialResult importUnityMaterial(const std::filesystem::path& matFilePath,
                                           const std::filesystem::path& packRoot,
                                           const std::filesystem::path& assetsRoot);
```

**`src/editor/assets/EditorMaterialImporter.cpp`**
- `canImportUnityMaterial()`: checks `.mat` extension and that file starts with `%YAML`
- `importUnityMaterial()`:
  1. Parse `.mat` YAML line-by-line to extract `m_Name`, texture GUIDs, scalar properties, color
  2. Build GUID map: scan `packRoot/Texture/**/*.png.meta` for `guid:` lines, map GUID -> `.png` path
  3. Resolve each texture slot's GUID to a file path
  4. Convert PascalCase name to snake_case with `qdp_` prefix
  5. Check if output `.material` already exists — abort if so
  6. Write `.material` file to `assetsRoot/materials/`
  7. Return result

The `.mat` YAML parsing is line-by-line string matching (no YAML library needed). The format is structured enough that we can extract what we need with simple prefix/pattern matching:
- `m_Name:` line for the material name
- `- _MainTex:` / `- _BumpMap:` / etc. to identify current texture slot
- `m_Texture: {fileID: ..., guid: ...,` to extract GUID
- `- _Metallic:` / `- _Color:` etc. for scalar/color values

### Editor Integration

**`src/editor/ui/EditorAssetBrowserPanel.cpp`** — add to the `default:` case in the context menu switch:

```cpp
case EditorAssetBrowserKind::Other:
    if (canImportUnityMaterial(node.absolutePath)) {
        if (ImGui::MenuItem("Import Material to Project")) {
            const auto packRoot = detectPackRoot(node.absolutePath);
            const auto result = importUnityMaterial(node.absolutePath, packRoot, assetsRoot);
            if (result.success) {
                spdlog::info("Imported material '{}' to {}", result.materialId, result.outputPath);
            } else {
                spdlog::error("Failed to import material: {}", result.errorMessage);
            }
        }
    }
    break;
```

`detectPackRoot()`: walk up from the `.mat` file until finding a directory that contains both `Material/` and `Texture/` subdirectories. For `assets/packs/QuestDoorsPack/Material/DoorA.mat`, this returns `assets/packs/QuestDoorsPack/`.

### ContentRegistry Reload

The existing hot-reload poll (500ms interval) will pick up the new `.material` file automatically. No explicit reload call needed.

## What This Does NOT Do

- No multi-select batch import (future enhancement)
- No preview dialog before import
- No support for non-Standard Unity shaders
- No height/parallax map import (engine doesn't support them)
- No emission map import (engine only has `emissive_strength` scalar)
