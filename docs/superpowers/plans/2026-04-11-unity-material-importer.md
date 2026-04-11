# Unity Material Importer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add right-click "Import Material to Project" for Unity `.mat` files in the editor asset browser, converting them to engine `.material` format with GUID-based texture resolution.

**Architecture:** New `EditorMaterialImporter.h/.cpp` handles all conversion logic (YAML parsing, GUID resolution, property mapping). The asset browser panel gets a small addition to the `default:` context menu case. Import result uses the existing `AssetBrowserActionResult.newMaterialId` path so the caller (main.cpp) picks it up and registers it with ContentRegistry automatically.

**Tech Stack:** C++20, ImGui, spdlog, std::filesystem. No new dependencies.

---

### Task 1: Create EditorMaterialImporter with GUID resolution and material conversion

**Files:**
- Create: `src/editor/assets/EditorMaterialImporter.h`
- Create: `src/editor/assets/EditorMaterialImporter.cpp`
- Modify: `src/editor/CMakeLists.txt` (add new source file)

- [ ] **Step 1: Create the header file**

Create `src/editor/assets/EditorMaterialImporter.h`:

```cpp
#pragma once

#include "game/rendering/MaterialDefinition.h"

#include <filesystem>
#include <string>

struct ImportedMaterialResult {
    bool success = false;
    std::string materialId;
    std::string outputPath;
    std::string errorMessage;
};

/// Returns true if the file is a Unity .mat file that can be imported.
bool canImportUnityMaterial(const std::filesystem::path& path);

/// Convert a Unity .mat file to an engine .material file.
/// Writes to assetsRoot/materials/{derived_name}.material.
ImportedMaterialResult importUnityMaterial(const std::filesystem::path& matFilePath,
                                           const std::filesystem::path& assetsRoot);
```

- [ ] **Step 2: Create the implementation file**

Create `src/editor/assets/EditorMaterialImporter.cpp`:

```cpp
#include "editor/assets/EditorMaterialImporter.h"

#include "engine/core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

// --- PascalCase to snake_case ---
// "DoorA" -> "door_a", "FuseBoxA" -> "fuse_box_a", "Cable_Red" -> "cable_red"
std::string pascalToSnake(const std::string& name) {
    std::string result;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (std::isupper(c) && i > 0 && name[i - 1] != '_' && std::islower(name[i - 1])) {
            result += '_';
        }
        result += static_cast<char>(std::tolower(c));
    }
    return result;
}

// --- Trim whitespace ---
std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// --- Extract value after "key: " from a YAML line ---
std::string yamlValue(const std::string& line, const std::string& key) {
    const auto pos = line.find(key + ":");
    if (pos == std::string::npos) return "";
    return trim(line.substr(pos + key.size() + 1));
}

// --- Extract a float from "- _Key: value" ---
bool parseFloat(const std::string& line, const std::string& key, float& out) {
    const std::string prefix = "- " + key + ": ";
    const auto pos = line.find(prefix);
    if (pos == std::string::npos) return false;
    try {
        out = std::stof(line.substr(pos + prefix.size()));
        return true;
    } catch (...) {
        return false;
    }
}

// --- Extract GUID from texture reference line ---
// Line looks like: "m_Texture: {fileID: 2800000, guid: abc123def, type: 3}"
// Returns empty string for null refs: "m_Texture: {fileID: 0}"
std::string extractGuid(const std::string& line) {
    if (line.find("fileID: 0}") != std::string::npos) return "";
    const std::string marker = "guid: ";
    const auto pos = line.find(marker);
    if (pos == std::string::npos) return "";
    const auto end = line.find(',', pos + marker.size());
    if (end == std::string::npos) return "";
    return line.substr(pos + marker.size(), end - pos - marker.size());
}

// --- Extract RGBA color from "_Color: {r: R, g: G, b: B, a: A}" ---
bool parseColor(const std::string& line, float& r, float& g, float& b) {
    const auto rPos = line.find("r: ");
    const auto gPos = line.find("g: ");
    const auto bPos = line.find("b: ");
    if (rPos == std::string::npos || gPos == std::string::npos || bPos == std::string::npos) return false;
    try {
        r = std::stof(line.substr(rPos + 3));
        g = std::stof(line.substr(gPos + 3));
        b = std::stof(line.substr(bPos + 3));
        return true;
    } catch (...) {
        return false;
    }
}

// --- Build GUID -> path map from .png.meta files ---
std::unordered_map<std::string, fs::path> buildGuidMap(const fs::path& textureRoot) {
    std::unordered_map<std::string, fs::path> guidMap;
    if (!fs::exists(textureRoot)) return guidMap;

    for (const auto& entry : fs::recursive_directory_iterator(textureRoot)) {
        if (!entry.is_regular_file()) continue;
        const auto& metaPath = entry.path();
        if (metaPath.extension() != ".meta") continue;
        // Only care about .png.meta files
        const std::string stem = metaPath.stem().string(); // e.g. "T_DoorA_BaseColor.png"
        if (stem.size() < 4 || stem.substr(stem.size() - 4) != ".png") continue;

        std::ifstream metaFile(metaPath);
        std::string line;
        while (std::getline(metaFile, line)) {
            if (line.find("guid:") != std::string::npos) {
                const std::string guid = trim(line.substr(line.find("guid:") + 5));
                if (!guid.empty()) {
                    // The texture path is the .meta path without the .meta extension
                    guidMap[guid] = metaPath.parent_path() / stem;
                }
                break;
            }
        }
    }
    return guidMap;
}

// --- Detect the pack root directory ---
// Walk up from the .mat file until finding a directory with both Material/ and Texture/ subdirectories.
fs::path detectPackRoot(const fs::path& matFilePath) {
    fs::path dir = matFilePath.parent_path();
    for (int i = 0; i < 5; ++i) {
        const fs::path parent = dir.parent_path();
        if (fs::exists(parent / "Material") && fs::exists(parent / "Texture")) {
            return parent;
        }
        dir = parent;
    }
    // Fallback: assume Material/ is a direct child of pack root
    return matFilePath.parent_path().parent_path();
}

// --- Make a path relative to the project root ---
std::string makeProjectRelative(const fs::path& absolutePath) {
    const fs::path root(projectRoot());
    auto rel = fs::relative(absolutePath, root);
    return rel.string();
}

} // namespace

bool canImportUnityMaterial(const fs::path& path) {
    if (path.extension() != ".mat") return false;
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string firstLine;
    std::getline(file, firstLine);
    return firstLine.find("%YAML") != std::string::npos;
}

ImportedMaterialResult importUnityMaterial(const fs::path& matFilePath,
                                           const fs::path& assetsRoot) {
    ImportedMaterialResult result;

    // 1. Read the .mat file
    std::ifstream matFile(matFilePath);
    if (!matFile.is_open()) {
        result.errorMessage = "Cannot open file: " + matFilePath.string();
        return result;
    }

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(matFile, line)) {
            lines.push_back(line);
        }
    }

    // 2. Parse material properties
    std::string matName;
    float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
    float metallic = 0.0f;
    float glossMapScale = 0.5f;
    float bumpScale = 1.0f;
    float occlusionStrength = 0.9f;
    bool hasColor = false;

    // Texture GUIDs by slot
    std::string guidMainTex;
    std::string guidBumpMap;
    std::string guidMetallicGlossMap;
    std::string guidOcclusionMap;

    // State machine for line-by-line YAML parsing
    enum class TexSlot { None, MainTex, BumpMap, MetallicGlossMap, OcclusionMap };
    TexSlot currentSlot = TexSlot::None;
    bool inColors = false;

    for (const auto& line : lines) {
        const std::string trimmed = trim(line);

        // Material name
        if (trimmed.find("m_Name:") != std::string::npos) {
            matName = trim(trimmed.substr(trimmed.find("m_Name:") + 7));
        }

        // Track texture slot sections
        if (trimmed.find("- _MainTex:") != std::string::npos) { currentSlot = TexSlot::MainTex; continue; }
        if (trimmed.find("- _BumpMap:") != std::string::npos) { currentSlot = TexSlot::BumpMap; continue; }
        if (trimmed.find("- _MetallicGlossMap:") != std::string::npos) { currentSlot = TexSlot::MetallicGlossMap; continue; }
        if (trimmed.find("- _OcclusionMap:") != std::string::npos) { currentSlot = TexSlot::OcclusionMap; continue; }
        // Reset slot on other texture sections
        if (trimmed.find("- _") == 0 && trimmed.find("Map:") != std::string::npos) { currentSlot = TexSlot::None; continue; }
        if (trimmed.find("- _") == 0 && trimmed.find("Tex:") != std::string::npos) { currentSlot = TexSlot::None; continue; }
        if (trimmed.find("- _") == 0 && trimmed.find("Mask:") != std::string::npos) { currentSlot = TexSlot::None; continue; }

        // Extract GUID from m_Texture line
        if (trimmed.find("m_Texture:") != std::string::npos && currentSlot != TexSlot::None) {
            const std::string guid = extractGuid(trimmed);
            switch (currentSlot) {
                case TexSlot::MainTex: guidMainTex = guid; break;
                case TexSlot::BumpMap: guidBumpMap = guid; break;
                case TexSlot::MetallicGlossMap: guidMetallicGlossMap = guid; break;
                case TexSlot::OcclusionMap: guidOcclusionMap = guid; break;
                default: break;
            }
            currentSlot = TexSlot::None;
        }

        // Scalar floats
        parseFloat(trimmed, "_Metallic", metallic);
        parseFloat(trimmed, "_GlossMapScale", glossMapScale);
        parseFloat(trimmed, "_BumpScale", bumpScale);
        parseFloat(trimmed, "_OcclusionStrength", occlusionStrength);

        // Color section
        if (trimmed == "m_Colors:") { inColors = true; continue; }
        if (inColors && trimmed.find("- _Color:") != std::string::npos) {
            hasColor = parseColor(trimmed, colorR, colorG, colorB);
            inColors = false;
        }
    }

    if (matName.empty()) {
        // Fallback: use filename stem
        matName = matFilePath.stem().string();
    }

    // 3. Derive output material ID
    const std::string snakeName = pascalToSnake(matName);
    const std::string materialId = "qdp_" + snakeName;
    const fs::path outputDir = assetsRoot / "materials";
    const fs::path outputPath = outputDir / (materialId + ".material");

    // 4. Check for duplicates
    if (fs::exists(outputPath)) {
        result.errorMessage = "Material '" + materialId + "' already exists at " + outputPath.string();
        spdlog::warn("Import skipped: {}", result.errorMessage);
        return result;
    }

    // 5. Resolve texture GUIDs
    const fs::path packRoot = detectPackRoot(matFilePath);
    const fs::path textureRoot = packRoot / "Texture";
    const auto guidMap = buildGuidMap(textureRoot);

    auto resolveTexture = [&](const std::string& guid) -> std::string {
        if (guid.empty()) return "";
        const auto it = guidMap.find(guid);
        if (it == guidMap.end()) {
            spdlog::warn("Could not resolve GUID {} for material '{}'", guid, matName);
            return "";
        }
        return makeProjectRelative(it->second);
    };

    const std::string albedoPath = resolveTexture(guidMainTex);
    const std::string normalPath = resolveTexture(guidBumpMap);
    const std::string roughnessPath = resolveTexture(guidMetallicGlossMap);
    const std::string aoPath = resolveTexture(guidOcclusionMap);

    // 6. Build MaterialDefinition
    MaterialDefinition def;
    def.id = materialId;
    if (hasColor) {
        def.baseColor = glm::vec3(colorR, colorG, colorB);
    } else {
        def.baseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    }
    def.uvMode = MaterialUvMode::Mesh;
    def.uvScale = glm::vec2(1.0f, 1.0f);
    def.normalStrength = bumpScale;
    def.roughnessScale = 1.0f;
    def.roughnessBias = 1.0f - glossMapScale;
    def.metalness = metallic;
    def.aoStrength = occlusionStrength;

    if (!albedoPath.empty()) def.albedoMapPath = albedoPath;
    if (!normalPath.empty()) def.normalMapPath = normalPath;
    if (!roughnessPath.empty()) def.roughnessMapPath = roughnessPath;
    if (!aoPath.empty()) def.aoMapPath = aoPath;

    // 7. Write .material file
    try {
        fs::create_directories(outputDir);
        saveMaterialDefinitionAsset(outputPath.string(), def);
    } catch (const std::exception& ex) {
        result.errorMessage = "Failed to write: " + std::string(ex.what());
        return result;
    }

    spdlog::info("Imported Unity material '{}' -> '{}'", matName, materialId);
    if (!albedoPath.empty()) spdlog::info("  albedo:    {}", albedoPath);
    if (!normalPath.empty()) spdlog::info("  normal:    {}", normalPath);
    if (!roughnessPath.empty()) spdlog::info("  roughness: {}", roughnessPath);
    if (!aoPath.empty()) spdlog::info("  ao:        {}", aoPath);

    result.success = true;
    result.materialId = materialId;
    result.outputPath = outputPath.string();
    return result;
}
```

- [ ] **Step 3: Add the new source file to CMakeLists.txt**

In `src/editor/CMakeLists.txt`, add `assets/EditorMaterialImporter.cpp` after the existing `assets/EditorAssetBrowser.cpp` line:

```cmake
add_library(editor STATIC
    assets/EditorAssetBrowser.cpp
    assets/EditorMaterialImporter.cpp
    build/EditorBuildSystem.cpp
    ...
```

- [ ] **Step 4: Build to verify compilation**

Run: `cmake --build build --target editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/editor/assets/EditorMaterialImporter.h src/editor/assets/EditorMaterialImporter.cpp src/editor/CMakeLists.txt
git commit -m "Add EditorMaterialImporter for Unity .mat to .material conversion"
```

---

### Task 2: Wire import into the asset browser context menu

**Files:**
- Modify: `src/editor/ui/EditorAssetBrowserPanel.cpp`

- [ ] **Step 1: Add the include**

At the top of `EditorAssetBrowserPanel.cpp`, add the include after the existing editor includes:

```cpp
#include "editor/assets/EditorMaterialImporter.h"
```

- [ ] **Step 2: Add context menu item for `.mat` files**

In the `if (ImGui::BeginPopupContextItem("AssetFileContext"))` block, find the `default: break;` case (around line 604) and replace it with:

```cpp
            default:
                if (canImportUnityMaterial(node.absolutePath)) {
                    if (ImGui::MenuItem("Import Material to Project")) {
                        const auto importResult = importUnityMaterial(
                            node.absolutePath,
                            resolveProjectPath("assets"));
                        if (importResult.success) {
                            result.newMaterialId = importResult.materialId;
                            result.assetCatalogChanged = true;
                            refreshAssetTree = true;
                        } else {
                            spdlog::error("Material import failed: {}", importResult.errorMessage);
                        }
                    }
                }
                break;
```

This uses `result.newMaterialId` which is already handled by the caller in `main.cpp` (lines 1173-1184) — it calls `loadMaterialDefinitionAsset`, `content.addMaterial()`, and refreshes `materialIds` + `materialTextures`. No changes needed in `main.cpp`.

- [ ] **Step 3: Build the full level-editor target**

Run: `cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/editor/ui/EditorAssetBrowserPanel.cpp
git commit -m "Add 'Import Material to Project' context menu for Unity .mat files"
```

---

### Task 3: Manual verification in the editor

- [ ] **Step 1: Launch the editor**

Run: `build/apps/level_editor/level-editor &`

- [ ] **Step 2: Verify context menu appears**

1. In the Asset Browser, navigate to `assets/packs/QuestDoorsPack/Material/`
2. Right-click on `DoorB.mat` (one that hasn't been imported yet)
3. Verify "Import Material to Project" menu item appears

- [ ] **Step 3: Verify import works**

1. Click "Import Material to Project" on `DoorB.mat`
2. Check that `assets/materials/qdp_door_b.material` was created
3. Verify the material appears in the materials list in the asset browser
4. Check the console log for the import confirmation message with texture paths

- [ ] **Step 4: Verify duplicate handling**

1. Right-click `DoorB.mat` again and click "Import Material to Project"
2. Check the console log for a warning that the material already exists
3. The existing file should not be overwritten

- [ ] **Step 5: Verify edge case — color-only material**

1. Right-click `Cable_Red.mat` and import it
2. Check `assets/materials/qdp_cable_red.material` — it should have:
   - `base_color 0.679 0.0 0.0` (red tint from `_Color`)
   - `normal_map` pointing to `Texture/Cable/T_Cable_Normal.png` (shared via GUID)
   - No `albedo_map` (Unity `.mat` has `fileID: 0` for `_MainTex`)

- [ ] **Step 6: Verify material is usable**

1. Select a mesh object in the scene
2. In the inspector, change its material to the newly imported `qdp_door_b`
3. Verify the material renders correctly with textures

- [ ] **Step 7: Commit the spec and plan documents**

```bash
git add docs/superpowers/specs/2026-04-11-unity-material-importer-design.md docs/superpowers/plans/2026-04-11-unity-material-importer.md
git commit -m "Add design spec and implementation plan for Unity material importer"
```
