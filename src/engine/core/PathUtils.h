#pragma once

#include <string>

std::string resolveProjectPath(const std::string& relativePath);

/// Returns true if the project root contains an assets/ directory.
/// Use this to validate the runtime environment before loading content.
bool hasValidProjectRoot();
