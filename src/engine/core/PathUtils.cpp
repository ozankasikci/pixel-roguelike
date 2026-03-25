#include "engine/core/PathUtils.h"

#include <filesystem>

std::string resolveProjectPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    const fs::path direct(relativePath);
    if (fs::exists(direct)) {
        return direct.lexically_normal().string();
    }

    fs::path probe = fs::current_path();
    for (int depth = 0; depth < 4; ++depth) {
        const fs::path candidate = (probe / relativePath).lexically_normal();
        if (fs::exists(candidate)) {
            return candidate.string();
        }
        if (!probe.has_parent_path()) {
            break;
        }
        probe = probe.parent_path();
    }

    return relativePath;
}
