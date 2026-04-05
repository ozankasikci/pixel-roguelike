#include "engine/core/PathUtils.h"

#include <filesystem>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace {

// Cached project root, resolved once on first call.
std::string g_projectRoot;

// Walk up from `start` looking for a directory that contains `assets/`.
std::string findProjectRoot(const std::filesystem::path& start) {
    namespace fs = std::filesystem;
    fs::path probe = start;
    for (int depth = 0; depth < 6; ++depth) {
        if (fs::exists(probe / "assets")) {
            return probe.lexically_normal().string();
        }
        if (!probe.has_parent_path() || probe == probe.parent_path()) {
            break;
        }
        probe = probe.parent_path();
    }
    return {};
}

const std::string& projectRoot() {
    if (!g_projectRoot.empty()) {
        return g_projectRoot;
    }

    namespace fs = std::filesystem;

    // 1. Try from current working directory
    std::string root = findProjectRoot(fs::current_path());
    if (!root.empty()) {
        g_projectRoot = root;
        return g_projectRoot;
    }

    // 2. Try from executable location (handles double-click / launch from Finder)
#if defined(__APPLE__)
    // _NSGetExecutablePath or /proc/self/exe equivalent
    char buf[4096];
    uint32_t bufSize = sizeof(buf);
    if (_NSGetExecutablePath(buf, &bufSize) == 0) {
        root = findProjectRoot(fs::path(buf).parent_path());
        if (!root.empty()) {
            g_projectRoot = root;
            return g_projectRoot;
        }
    }
#elif defined(_WIN32)
    char buf[4096];
    DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (len > 0 && len < sizeof(buf)) {
        root = findProjectRoot(fs::path(buf).parent_path());
        if (!root.empty()) {
            g_projectRoot = root;
            return g_projectRoot;
        }
    }
#elif defined(__linux__)
    auto exePath = fs::read_symlink("/proc/self/exe");
    root = findProjectRoot(exePath.parent_path());
    if (!root.empty()) {
        g_projectRoot = root;
        return g_projectRoot;
    }
#endif

    // Fallback: use cwd
    g_projectRoot = fs::current_path().string();
    return g_projectRoot;
}

} // namespace

std::string resolveProjectPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    // If the path is already absolute and exists, use it directly
    const fs::path direct(relativePath);
    if (direct.is_absolute() && fs::exists(direct)) {
        return direct.lexically_normal().string();
    }

    // Resolve relative to the detected project root
    const fs::path candidate = (fs::path(projectRoot()) / relativePath).lexically_normal();
    if (fs::exists(candidate)) {
        return candidate.string();
    }

    return relativePath;
}
