#include "editor/build/EditorBuildSystem.h"

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

#include "engine/rendering/assets/ModelLoader.h"

// ---------------------------------------------------------------------------
// classifyLine
// ---------------------------------------------------------------------------

BuildLineKind classifyLine(const std::string& line, float& progressOut) {
    int pct = 0;
    if (std::sscanf(line.c_str(), "[ %d%%]", &pct) == 1 ||
        std::sscanf(line.c_str(), "[%d%%]", &pct) == 1) {
        progressOut = static_cast<float>(pct);
        return BuildLineKind::Progress;
    }
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("error:") != std::string::npos) return BuildLineKind::Error;
    if (lower.find("warning:") != std::string::npos) return BuildLineKind::Warning;
    return BuildLineKind::Normal;
}

#ifndef _WIN32
// ---------------------------------------------------------------------------
// Internal reader thread
// ---------------------------------------------------------------------------

static void readerThreadFn(int fd,
                           std::mutex& mtx,
                           std::vector<std::string>& queue,
                           std::atomic<bool>& done) {
    std::string partial;
    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            partial.append(buf, static_cast<size_t>(n));
            size_t pos;
            while ((pos = partial.find('\n')) != std::string::npos) {
                std::string line = partial.substr(0, pos);
                partial.erase(0, pos + 1);
                std::lock_guard<std::mutex> lock(mtx);
                queue.push_back(std::move(line));
            }
        } else if (n == -1 && errno == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        } else {
            // n == 0: EOF (process exited), or unrecoverable error
            // Flush any remaining partial line (no trailing newline)
            if (!partial.empty()) {
                std::lock_guard<std::mutex> lock(mtx);
                queue.push_back(std::move(partial));
            }
            break;
        }
    }
    done = true;
}

// ---------------------------------------------------------------------------
// Internal helper: common fork+exec logic
// ---------------------------------------------------------------------------

static void spawnProcess(EditorBuildState& state, std::vector<std::string> argStrings) {
    // Build null-terminated argv from argStrings (pointers remain valid during execvp call)
    std::vector<const char*> argv;
    argv.reserve(argStrings.size() + 1);
    for (auto& s : argStrings) {
        argv.push_back(s.c_str());
    }
    argv.push_back(nullptr);

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        spdlog::error("EditorBuildSystem: pipe() failed: {}", std::strerror(errno));
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("EditorBuildSystem: fork() failed: {}", std::strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        // --- Child process ---
        setpgid(0, 0);  // New process group so SIGTERM reaches all subprocesses (D-14)
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        // argv[0] is the executable name (e.g. "cmake")
        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(1);  // exec failed
    }

    // --- Parent process ---
    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    state.pid         = pid;
    state.pipeFd      = pipefd[0];
    state.running     = true;
    state.progressPct = 0.0f;
    state.readerDone  = false;
    state.exitCode    = -1;

    state.readerThread = std::thread(
        readerThreadFn,
        pipefd[0],
        std::ref(state.queueMutex),
        std::ref(state.pendingLines),
        std::ref(state.readerDone));
}

// ---------------------------------------------------------------------------
// startBuild
// ---------------------------------------------------------------------------

void startBuild(EditorBuildState& state, const EditorBuildConfig& config, const std::string& target) {
    spdlog::info("EditorBuildSystem: starting build — target={} dir={} config={}",
                 target, config.buildDir, config.buildConfig);

    std::vector<std::string> args = {
        "cmake",
        "--build",
        config.buildDir,
        "--target",
        target,
        "--parallel"
    };
    spawnProcess(state, std::move(args));
}

// ---------------------------------------------------------------------------
// startConfigure
// ---------------------------------------------------------------------------

void startConfigure(EditorBuildState& state, const EditorBuildConfig& config) {
    spdlog::info("EditorBuildSystem: starting configure — dir={} config={}",
                 config.buildDir, config.buildConfig);

    std::string cmakeBuildType = "-DCMAKE_BUILD_TYPE=" + config.buildConfig;

    std::vector<std::string> args = {
        "cmake",
        "-S",
        ".",
        "-B",
        config.buildDir,
        cmakeBuildType
    };
    spawnProcess(state, std::move(args));
}

// ---------------------------------------------------------------------------
// cancelBuild
// ---------------------------------------------------------------------------

void cancelBuild(EditorBuildState& state) {
    if (state.pid > 0) {
        spdlog::info("EditorBuildSystem: cancelling build (pid={})", state.pid);
        kill(-state.pid, SIGTERM);  // Negative PID -> entire process group (D-14)
        // running = false is handled by waitpid in pollBuild
    }
}

// ---------------------------------------------------------------------------
// pollBuild
// ---------------------------------------------------------------------------

void pollBuild(EditorBuildState& state, BuildOutputLog& log) {
    if (!state.running) return;

    // Drain the pending line queue (lock-swap pattern)
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(state.queueMutex);
        lines.swap(state.pendingLines);
    }
    for (const auto& line : lines) {
        float pct = 0.0f;
        BuildLineKind kind = classifyLine(line, pct);
        if (kind == BuildLineKind::Progress && pct > 0.0f) {
            state.progressPct = pct;
        }
        log.addLine(line.c_str(), kind);
    }

    // Non-blocking waitpid to detect process exit
    int status = 0;
    pid_t result = waitpid(state.pid, &status, WNOHANG);
    if (result == state.pid) {
        state.running = false;
        state.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        spdlog::info("EditorBuildSystem: build finished — exitCode={}", state.exitCode);

        state.pid = -1;

        if (state.readerThread.joinable()) {
            state.readerThread.join();
        }
        close(state.pipeFd);
        state.pipeFd = -1;

        // Drain any lines that arrived after waitpid (between last poll and now)
        {
            std::lock_guard<std::mutex> lock(state.queueMutex);
            lines.swap(state.pendingLines);
        }
        for (const auto& line : lines) {
            float pct = 0.0f;
            BuildLineKind kind = classifyLine(line, pct);
            if (kind == BuildLineKind::Progress && pct > 0.0f) {
                state.progressPct = pct;
            }
            log.addLine(line.c_str(), kind);
        }

        if (state.exitCode != 0) {
            log.scrollToError = true;
        }
    }
}

// ---------------------------------------------------------------------------
// launchGame
// ---------------------------------------------------------------------------

void launchGame(const EditorBuildConfig& config, const std::string& scenePath) {
    std::string binaryPath = gameBinaryPath(config);
    spdlog::info("EditorBuildSystem: launching game binary={} scene={}", binaryPath, scenePath);

    std::vector<std::string> argStrings;
    argStrings.push_back(binaryPath);
    if (!scenePath.empty()) {
        argStrings.push_back("--scene");
        argStrings.push_back(scenePath);
    }

    std::vector<const char*> argv;
    argv.reserve(argStrings.size() + 1);
    for (auto& s : argStrings) {
        argv.push_back(s.c_str());
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("EditorBuildSystem: launchGame fork() failed: {}", std::strerror(errno));
        return;
    }
    if (pid == 0) {
        // Child: new process group, fire-and-forget
        setpgid(0, 0);
        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(1);
    }
    // Parent: do not wait — fire-and-forget
    spdlog::info("EditorBuildSystem: game launched (pid={})", pid);
}

#else // _WIN32 stubs

void startBuild(EditorBuildState& state, const EditorBuildConfig& config, const std::string& target) {
    (void)state; (void)config; (void)target;
    spdlog::warn("EditorBuildSystem: not implemented on Windows");
}

void startConfigure(EditorBuildState& state, const EditorBuildConfig& config) {
    (void)state; (void)config;
    spdlog::warn("EditorBuildSystem: not implemented on Windows");
}

void cancelBuild(EditorBuildState& state) {
    (void)state;
}

void pollBuild(EditorBuildState& state, BuildOutputLog& log) {
    (void)state; (void)log;
}

void launchGame(const EditorBuildConfig& config, const std::string& scenePath) {
    (void)config; (void)scenePath;
    spdlog::warn("EditorBuildSystem: launchGame not implemented on Windows");
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// collectUsedAssets
// ---------------------------------------------------------------------------

namespace fs = std::filesystem;

/// Helper: scan all files matching an extension under a directory (optionally recursive).
static std::vector<fs::path> findFiles(const fs::path& dir, const std::string& ext,
                                       bool recursive = false) {
    std::vector<fs::path> result;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        return result;
    }
    if (recursive) {
        for (auto& entry : fs::recursive_directory_iterator(
                 dir, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ext) {
                result.push_back(entry.path());
            }
        }
    } else {
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ext) {
                result.push_back(entry.path());
            }
        }
    }
    return result;
}

AssetManifest collectUsedAssets() {
    AssetManifest manifest;
    std::set<std::string> neededMeshIds;

    // 1. Collect texture paths from material files
    for (const auto& matFile : findFiles("assets/materials", ".material")) {
        std::ifstream in(matFile);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string key, value;
            if (!(iss >> key >> value)) {
                continue;
            }
            if (key == "albedo_map" || key == "normal_map" ||
                key == "roughness_map" || key == "ao_map") {
                manifest.texturePaths.insert(value);
            }
        }
    }

    // 2. Collect sky/cloud asset paths from environment files
    auto envDirs = {"assets/defs/environments", "assets/environments"};
    for (const auto& dir : envDirs) {
        for (const auto& envFile : findFiles(dir, ".environment")) {
            std::ifstream in(envFile);
            std::string line;
            while (std::getline(in, line)) {
                std::istringstream iss(line);
                std::string key;
                if (!(iss >> key)) {
                    continue;
                }
                if (key == "sky_cubemap_faces") {
                    // 6 face paths follow
                    std::string face;
                    for (int i = 0; i < 6; ++i) {
                        if (iss >> face) {
                            manifest.skyPaths.insert(face);
                        }
                    }
                } else if (key == "sky_cloud_layer_a" || key == "sky_cloud_layer_b" ||
                           key == "sky_panorama_path" || key == "sky_horizon_layer") {
                    std::string path;
                    if (iss >> path) {
                        manifest.skyPaths.insert(path);
                    }
                }
            }
        }
    }

    // 3. Collect mesh IDs from scene files
    for (const auto& sceneFile : findFiles("assets/scenes", ".scene")) {
        std::ifstream in(sceneFile);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string key, meshId;
            if (!(iss >> key >> meshId)) {
                continue;
            }
            if (key == "mesh") {
                neededMeshIds.insert(meshId);
            }
        }
    }

    // 4. Collect mesh IDs from prefab files
    for (const auto& prefabFile : findFiles("assets/prefabs", ".prefab", true)) {
        std::ifstream in(prefabFile);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string key, value;
            if (!(iss >> key >> value)) {
                continue;
            }
            if (key == "mesh" || key == "left_leaf_mesh" || key == "right_leaf_mesh") {
                neededMeshIds.insert(value);
            }
        }
    }

    // 5. Collect mesh IDs from def files (.weapon, .enemy, .item, .skill)
    auto defExts = {".weapon", ".enemy", ".item", ".skill"};
    for (const auto& ext : defExts) {
        for (const auto& defFile : findFiles("assets/defs", ext, true)) {
            std::ifstream in(defFile);
            std::string line;
            while (std::getline(in, line)) {
                std::istringstream iss(line);
                std::string key, value;
                if (!(iss >> key >> value)) {
                    continue;
                }
                if (key == "mesh") {
                    neededMeshIds.insert(value);
                }
            }
        }
    }

    // 6. Resolve mesh IDs to file paths using ModelLoader
    auto meshesDiscovered = ModelLoader::discoverProjectAssets("assets/meshes", fs::current_path());
    auto packsDiscovered = ModelLoader::discoverProjectAssets("assets/packs", fs::current_path());

    // Merge both lists
    std::vector<DiscoveredModelAsset> allAssets;
    allAssets.insert(allAssets.end(), meshesDiscovered.begin(), meshesDiscovered.end());
    allAssets.insert(allAssets.end(), packsDiscovered.begin(), packsDiscovered.end());

    std::set<std::string> resolvedIds;
    for (const auto& asset : allAssets) {
        if (neededMeshIds.count(asset.meshId)) {
            manifest.meshFiles.insert(asset.relativePath);
            resolvedIds.insert(asset.meshId);
        }
    }

    // Warn about unresolved mesh IDs
    for (const auto& id : neededMeshIds) {
        if (!resolvedIds.count(id)) {
            // Skip built-in procedural meshes (plane, cube, sphere, cylinder, quad, etc.)
            static const std::set<std::string> kBuiltinMeshes = {
                "plane", "cube", "sphere", "cylinder", "quad", "cone", "hemisphere"};
            if (!kBuiltinMeshes.count(id)) {
                spdlog::warn("collectUsedAssets: mesh ID '{}' referenced but not found on disk",
                             id);
            }
        }
    }

    spdlog::info(
        "collectUsedAssets: {} textures, {} sky assets, {} mesh files (from {} unique mesh IDs)",
        manifest.texturePaths.size(), manifest.skyPaths.size(), manifest.meshFiles.size(),
        resolvedIds.size());

    return manifest;
}

// ---------------------------------------------------------------------------
// packageGame
// ---------------------------------------------------------------------------

bool packageGame(const EditorBuildConfig& config, const std::string& outputDir,
                 BuildOutputLog& log) {
#ifdef _WIN32
    spdlog::warn("EditorBuildSystem: packageGame not implemented on Windows");
    log.addLine("Package: not implemented on Windows", BuildLineKind::Warning);
    return false;
#else
    log.addLine("Starting package...", BuildLineKind::Normal);

    // --- Copy game binary ---
    std::string binaryPath = gameBinaryPath(config);
    if (!fs::exists(binaryPath)) {
        std::string msg = "Package: game binary not found at " + binaryPath;
        spdlog::error("{}", msg);
        log.addLine(msg.c_str(), BuildLineKind::Error);
        return false;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::string msg = "Package: failed to create output dir: " + ec.message();
        log.addLine(msg.c_str(), BuildLineKind::Error);
        return false;
    }

    // Copy binary
    fs::path destBinary = fs::path(outputDir) / fs::path(binaryPath).filename();
    fs::copy_file(binaryPath, destBinary, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::string msg = "Package: failed to copy binary: " + ec.message();
        log.addLine(msg.c_str(), BuildLineKind::Error);
        return false;
    }
    log.addLine(("Copied binary: " + destBinary.string()).c_str(), BuildLineKind::Normal);

    // --- Create output assets directory ---
    fs::path outAssets = fs::path(outputDir) / "assets";
    fs::create_directories(outAssets, ec);

    // --- Always-copy directories (small, all needed) ---
    auto alwaysCopyDirs = {"shaders", "defs", "prefabs", "materials", "scenes", "fonts",
                           "environments"};
    for (const auto& subdir : alwaysCopyDirs) {
        fs::path src = fs::path("assets") / subdir;
        fs::path dst = outAssets / subdir;
        if (fs::exists(src, ec)) {
            fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                     ec);
            if (ec) {
                std::string msg =
                    "Package: warning copying " + src.string() + ": " + ec.message();
                log.addLine(msg.c_str(), BuildLineKind::Warning);
                ec.clear();
            }
        }
    }

    // --- Copy project.cfg if it exists ---
    if (fs::exists("assets/project.cfg", ec)) {
        fs::copy_file("assets/project.cfg", outAssets / "project.cfg",
                      fs::copy_options::overwrite_existing, ec);
    }

    // --- Collect and copy referenced assets ---
    AssetManifest manifest = collectUsedAssets();

    int fileCount = 0;
    uintmax_t totalBytes = 0;

    auto copyAsset = [&](const std::string& relPath) {
        try {
            fs::path dest = fs::path(outputDir) / relPath;
            fs::create_directories(dest.parent_path(), ec);
            if (fs::exists(relPath)) {
                fs::copy_file(relPath, dest, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    totalBytes += fs::file_size(relPath);
                    ++fileCount;
                } else {
                    std::string msg =
                        "Package: warning copying " + relPath + ": " + ec.message();
                    log.addLine(msg.c_str(), BuildLineKind::Warning);
                    ec.clear();
                }
            } else {
                std::string msg = "Package: referenced asset not found: " + relPath;
                log.addLine(msg.c_str(), BuildLineKind::Warning);
            }
        } catch (const std::exception& e) {
            std::string msg =
                "Package: exception copying " + relPath + ": " + std::string(e.what());
            log.addLine(msg.c_str(), BuildLineKind::Warning);
        }
    };

    for (const auto& path : manifest.texturePaths) {
        copyAsset(path);
    }
    for (const auto& path : manifest.skyPaths) {
        copyAsset(path);
    }
    for (const auto& path : manifest.meshFiles) {
        copyAsset(path);
    }

    // --- Compute total package size ---
    uintmax_t packageSize = 0;
    for (auto& entry : fs::recursive_directory_iterator(outputDir, ec)) {
        if (entry.is_regular_file()) {
            packageSize += entry.file_size();
        }
    }

    double packageMB = static_cast<double>(packageSize) / (1024.0 * 1024.0);
    double assetMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);

    char summary[256];
    std::snprintf(summary, sizeof(summary),
                  "Package created: %d referenced assets (%.1f MB assets, %.1f MB total package)",
                  fileCount, assetMB, packageMB);
    spdlog::info("{}", summary);
    log.addLine(summary, BuildLineKind::Normal);

    return true;
#endif
}

// ---------------------------------------------------------------------------
// needsConfigure
// ---------------------------------------------------------------------------

bool needsConfigure(const EditorBuildConfig& config) {
    return !std::filesystem::exists(config.buildDir + "/CMakeCache.txt");
}

// ---------------------------------------------------------------------------
// gameBinaryPath
// ---------------------------------------------------------------------------

std::string gameBinaryPath(const EditorBuildConfig& config) {
    return config.buildDir + "/apps/runtime/pixel-roguelike";
}

// ---------------------------------------------------------------------------
// loadBuildConfig / saveBuildConfig
// ---------------------------------------------------------------------------

void loadBuildConfig(EditorBuildConfig& config, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    char buf[512];
    while (std::getline(file, line)) {
        if (std::sscanf(line.c_str(), "build_dir=%511s", buf) == 1) {
            config.buildDir = buf;
        } else if (std::sscanf(line.c_str(), "build_config=%511s", buf) == 1) {
            config.buildConfig = buf;
        }
    }
}

void saveBuildConfig(const EditorBuildConfig& config, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::warn("EditorBuildSystem: could not save build config to {}", path);
        return;
    }
    file << "build_dir=" << config.buildDir << "\n";
    file << "build_config=" << config.buildConfig << "\n";
}
