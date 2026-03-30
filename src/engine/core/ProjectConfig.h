#pragma once

#include <string>

/// Read the last_scene value from a project.cfg file (key=value format).
/// Returns empty string if file does not exist or key is not present.
std::string readProjectCfgLastScene(const std::string& cfgPath);

/// Write the last_scene value to a project.cfg file.
/// Overwrites the entire file (file is small, one key currently).
void writeProjectCfgLastScene(const std::string& cfgPath, const std::string& sceneFilename);
