#include "engine/core/PathUtils.h"

#include <cstdlib>
#include <filesystem>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <spdlog/spdlog.h>

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

    // Strategy 1: PIXEL_ROGUELIKE_ROOT env var override (all platforms)
    const char* envRoot = std::getenv("PIXEL_ROGUELIKE_ROOT");
    if (envRoot) {
        fs::path envPath(envRoot);
        if (fs::exists(envPath / "assets")) {
            g_projectRoot = envPath.lexically_normal().string();
            spdlog::info("[PathUtils] Project root from PIXEL_ROGUELIKE_ROOT: {}",
                         g_projectRoot);
            return g_projectRoot;
        }
        spdlog::warn("[PathUtils] PIXEL_ROGUELIKE_ROOT='{}' set but no assets/ found there",
                     envRoot);
    }

    // Strategy 2: CWD walk-up (kept first after env var for dev ergonomics)
    std::string root = findProjectRoot(fs::current_path());
    if (!root.empty()) {
        g_projectRoot = root;
        spdlog::info("[PathUtils] Project root from CWD walk-up: {}", g_projectRoot);
        return g_projectRoot;
    }

    // Strategy 3: macOS .app bundle detection (Apple only)
    // Strategy 4: Exe-relative walk-up (all platforms)
#if defined(__APPLE__)
    char buf[4096];
    uint32_t bufSize = sizeof(buf);
    if (_NSGetExecutablePath(buf, &bufSize) == 0) {
        fs::path exePath = fs::weakly_canonical(fs::path(buf));

        // Strategy 3: Check for .app bundle structure
        std::string pathStr = exePath.string();
        auto appPos = pathStr.find(".app/Contents/MacOS");
        if (appPos != std::string::npos) {
            fs::path bundlePath = pathStr.substr(0, appPos + 4); // includes ".app"
            fs::path resources = bundlePath / "Contents" / "Resources";
            if (fs::exists(resources / "assets")) {
                g_projectRoot = resources.lexically_normal().string();
                spdlog::info("[PathUtils] Project root from macOS bundle: {}",
                             g_projectRoot);
                return g_projectRoot;
            }
        }

        // Strategy 4: Exe-relative walk-up
        root = findProjectRoot(exePath.parent_path());
        if (!root.empty()) {
            g_projectRoot = root;
            spdlog::info("[PathUtils] Project root from exe walk-up: {}", g_projectRoot);
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
            spdlog::info("[PathUtils] Project root from exe walk-up: {}", g_projectRoot);
            return g_projectRoot;
        }
    }
#elif defined(__linux__)
    auto exePath = fs::read_symlink("/proc/self/exe");
    root = findProjectRoot(exePath.parent_path());
    if (!root.empty()) {
        g_projectRoot = root;
        spdlog::info("[PathUtils] Project root from exe walk-up: {}", g_projectRoot);
        return g_projectRoot;
    }
#endif

    // Strategy 5: Fallback to CWD (no assets/ found anywhere)
    g_projectRoot = fs::current_path().string();
    spdlog::warn("[PathUtils] No assets/ found in any search path, falling back to CWD: {}",
                 g_projectRoot);
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

bool hasValidProjectRoot() {
    namespace fs = std::filesystem;
    const std::string resolved = resolveProjectPath("assets");
    return fs::exists(resolved) && fs::is_directory(resolved);
}
