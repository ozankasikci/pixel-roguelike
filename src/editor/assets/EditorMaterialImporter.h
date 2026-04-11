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
