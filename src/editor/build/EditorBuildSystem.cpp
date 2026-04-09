#include "editor/build/EditorBuildSystem.h"
#include "engine/core/PathUtils.h"

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
        // Ensure cmake runs from the project root so relative paths (build dir, assets) work
        if (chdir(resolveProjectPath(".").c_str()) != 0) {
            _exit(1);
        }
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
// collectUsedAssets (with optional scene filter)
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

// ---------------------------------------------------------------------------
// listBuildableScenes
// ---------------------------------------------------------------------------

std::vector<std::string> listBuildableScenes() {
    std::vector<std::string> scenes;
    for (const auto& path : findFiles(resolveProjectPath("assets/scenes"), ".scene")) {
        scenes.push_back(path.filename().string());
    }
    std::sort(scenes.begin(), scenes.end());
    return scenes;
}

AssetManifest collectUsedAssets(const std::vector<std::string>& sceneFilter) {
    AssetManifest manifest;
    std::set<std::string> neededMeshIds;

    // 1. Parse scene files: collect mesh IDs, material IDs, and environment IDs
    for (const auto& sceneFile : findFiles(resolveProjectPath("assets/scenes"), ".scene")) {
        if (!sceneFilter.empty()) {
            const std::string filename = sceneFile.filename().string();
            if (std::find(sceneFilter.begin(), sceneFilter.end(), filename) == sceneFilter.end()) {
                continue;
            }
        }
        std::ifstream in(sceneFile);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string key, value;
            if (!(iss >> key >> value)) {
                continue;
            }
            if (key == "mesh") {
                neededMeshIds.insert(value);
                // Scan rest of line for "material <id>"
                std::string token;
                while (iss >> token) {
                    if (token == "material" && (iss >> token)) {
                        manifest.materialIds.insert(token);
                    }
                }
            } else if (key == "environment_profile") {
                manifest.environmentIds.insert(value);
            }
        }
    }

    // 2. Resolve material parent chains — if material A inherits from B, include B too
    {
        std::string matDir = resolveProjectPath("assets/materials");
        std::set<std::string> pending = manifest.materialIds;
        while (!pending.empty()) {
            std::set<std::string> newParents;
            for (const auto& matId : pending) {
                fs::path matPath = fs::path(matDir) / (matId + ".material");
                if (!fs::exists(matPath)) continue;
                std::ifstream in(matPath);
                std::string line;
                while (std::getline(in, line)) {
                    std::istringstream iss(line);
                    std::string key, parentId;
                    if ((iss >> key >> parentId) && key == "parent") {
                        if (!manifest.materialIds.count(parentId)) {
                            manifest.materialIds.insert(parentId);
                            newParents.insert(parentId);
                        }
                    }
                }
            }
            pending = newParents;
        }
    }

    // 3. Collect texture paths only from referenced materials
    {
        std::string matDir = resolveProjectPath("assets/materials");
        for (const auto& matId : manifest.materialIds) {
            fs::path matPath = fs::path(matDir) / (matId + ".material");
            if (!fs::exists(matPath)) continue;
            std::ifstream in(matPath);
            std::string line;
            while (std::getline(in, line)) {
                std::istringstream iss(line);
                std::string key, value;
                if (!(iss >> key >> value)) continue;
                if (key == "albedo_map" || key == "normal_map" ||
                    key == "roughness_map" || key == "ao_map") {
                    manifest.texturePaths.insert(value);
                }
            }
        }
    }

    // 4. Collect sky/cloud asset paths only from referenced environments
    {
        std::vector<std::string> envDirs = {
            resolveProjectPath("assets/defs/environments"),
            resolveProjectPath("assets/environments")
        };
        for (const auto& dir : envDirs) {
            for (const auto& envFile : findFiles(dir, ".environment")) {
                std::string envId = envFile.stem().string();
                if (!manifest.environmentIds.count(envId)) continue;
                std::ifstream in(envFile);
                std::string line;
                while (std::getline(in, line)) {
                    std::istringstream iss(line);
                    std::string key;
                    if (!(iss >> key)) continue;
                    if (key == "sky_cubemap_faces") {
                        std::string face;
                        for (int i = 0; i < 6; ++i) {
                            if (iss >> face) manifest.skyPaths.insert(face);
                        }
                    } else if (key == "sky_cloud_layer_a" || key == "sky_cloud_layer_b" ||
                               key == "sky_panorama_path" || key == "sky_horizon_layer") {
                        std::string path;
                        if (iss >> path) manifest.skyPaths.insert(path);
                    }
                }
            }
        }
    }

    // 4. Collect mesh IDs from prefab files
    for (const auto& prefabFile : findFiles(resolveProjectPath("assets/prefabs"), ".prefab", true)) {
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
        for (const auto& defFile : findFiles(resolveProjectPath("assets/defs"), ext, true)) {
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
    fs::path projectDir = resolveProjectPath(".");
    auto meshesDiscovered = ModelLoader::discoverProjectAssets(resolveProjectPath("assets/meshes"), projectDir);
    auto packsDiscovered = ModelLoader::discoverProjectAssets(resolveProjectPath("assets/packs"), projectDir);

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

    // --- Create .app bundle structure ---
    fs::path appBundle = fs::path(outputDir) / "PixelRoguelike.app";
    fs::path contentsDir = appBundle / "Contents";
    fs::path macosDir = contentsDir / "MacOS";
    fs::path resourcesDir = contentsDir / "Resources";

    // Clean previous bundle if it exists
    if (fs::exists(appBundle, ec)) {
        fs::remove_all(appBundle, ec);
    }

    fs::create_directories(macosDir, ec);
    if (ec) {
        std::string msg = "Package: failed to create .app bundle: " + ec.message();
        log.addLine(msg.c_str(), BuildLineKind::Error);
        return false;
    }
    fs::create_directories(resourcesDir, ec);

    // Write Info.plist
    {
        std::ofstream plist(contentsDir / "Info.plist");
        plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
              << "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
              << "<plist version=\"1.0\">\n<dict>\n"
              << "    <key>CFBundleExecutable</key>\n"
              << "    <string>pixel-roguelike</string>\n"
              << "    <key>CFBundleIdentifier</key>\n"
              << "    <string>com.pixelroguelike.game</string>\n"
              << "    <key>CFBundleName</key>\n"
              << "    <string>Pixel Roguelike</string>\n"
              << "    <key>CFBundleVersion</key>\n"
              << "    <string>1.0</string>\n"
              << "    <key>CFBundleShortVersionString</key>\n"
              << "    <string>1.0</string>\n"
              << "    <key>CFBundlePackageType</key>\n"
              << "    <string>APPL</string>\n"
              << "    <key>NSHighResolutionCapable</key>\n"
              << "    <true/>\n"
              << "</dict>\n</plist>\n";
    }
    log.addLine("Created Info.plist", BuildLineKind::Normal);

    // Copy binary into Contents/MacOS/
    fs::path destBinary = macosDir / fs::path(binaryPath).filename();
    fs::copy_file(binaryPath, destBinary, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::string msg = "Package: failed to copy binary: " + ec.message();
        log.addLine(msg.c_str(), BuildLineKind::Error);
        return false;
    }
    log.addLine(("Copied binary: " + destBinary.string()).c_str(), BuildLineKind::Normal);

    // --- Create output assets directory in Resources ---
    fs::path outAssets = resourcesDir / "assets";
    fs::create_directories(outAssets, ec);

    // --- Always-copy directories (shaders and fonts are always needed) ---
    auto alwaysCopyDirs = {"shaders", "fonts"};
    for (const auto& subdir : alwaysCopyDirs) {
        fs::path src = fs::path(resolveProjectPath("assets")) / subdir;
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
    {
        std::string cfgPath = resolveProjectPath("assets/project.cfg");
        if (fs::exists(cfgPath, ec)) {
            fs::copy_file(cfgPath, outAssets / "project.cfg",
                          fs::copy_options::overwrite_existing, ec);
        }
    }

    // --- Copy scenes selectively ---
    {
        auto allScenes = listBuildableScenes();
        const std::vector<std::string>& scenesToCopy =
            config.buildScenes.empty() ? allScenes : config.buildScenes;

        int includedCount = static_cast<int>(scenesToCopy.size());
        int totalCount    = static_cast<int>(allScenes.size());

        if (includedCount == 0) {
            log.addLine("Package: warning — no scenes selected, package will contain no scenes",
                        BuildLineKind::Warning);
        } else {
            char sceneMsg[128];
            std::snprintf(sceneMsg, sizeof(sceneMsg), "Package: including %d of %d scenes",
                          includedCount, totalCount);
            log.addLine(sceneMsg, BuildLineKind::Normal);
        }

        fs::path scenesOut = outAssets / "scenes";
        fs::create_directories(scenesOut, ec);
        for (const auto& sceneName : scenesToCopy) {
            fs::path src = fs::path(resolveProjectPath("assets/scenes")) / sceneName;
            if (fs::exists(src, ec)) {
                fs::copy_file(src, scenesOut / sceneName,
                              fs::copy_options::overwrite_existing, ec);
            }
        }
    }

    // --- Collect and copy referenced assets ---
    AssetManifest manifest = collectUsedAssets(config.buildScenes);

    // --- Copy only referenced material files ---
    {
        fs::path matSrc = fs::path(resolveProjectPath("assets/materials"));
        fs::path matDst = outAssets / "materials";
        fs::create_directories(matDst, ec);
        for (const auto& matId : manifest.materialIds) {
            fs::path src = matSrc / (matId + ".material");
            if (fs::exists(src, ec)) {
                fs::copy_file(src, matDst / (matId + ".material"),
                              fs::copy_options::overwrite_existing, ec);
            }
        }
        char matMsg[128];
        std::snprintf(matMsg, sizeof(matMsg), "Package: %d of %d materials",
                      static_cast<int>(manifest.materialIds.size()),
                      static_cast<int>(findFiles(matSrc, ".material").size()));
        log.addLine(matMsg, BuildLineKind::Normal);
    }

    // --- Copy only referenced environment files ---
    {
        std::vector<std::pair<fs::path, fs::path>> envDirs = {
            {resolveProjectPath("assets/environments"), outAssets / "environments"},
            {resolveProjectPath("assets/defs/environments"), outAssets / "defs" / "environments"}
        };
        for (const auto& [srcDir, dstDir] : envDirs) {
            if (!fs::exists(srcDir, ec)) continue;
            fs::create_directories(dstDir, ec);
            for (const auto& envId : manifest.environmentIds) {
                fs::path src = srcDir / (envId + ".environment");
                if (fs::exists(src, ec)) {
                    fs::copy_file(src, dstDir / (envId + ".environment"),
                                  fs::copy_options::overwrite_existing, ec);
                }
            }
        }
    }

    // --- Copy defs referenced by scenes (archetypes, weapons, enemies, etc.) ---
    // For now, copy the entire defs directory minus environments (already handled above).
    // Def files are small text files — not worth the complexity of per-file filtering yet.
    {
        fs::path defsSrc = fs::path(resolveProjectPath("assets/defs"));
        fs::path defsDst = outAssets / "defs";
        if (fs::exists(defsSrc, ec)) {
            for (auto& entry : fs::recursive_directory_iterator(defsSrc, ec)) {
                if (!entry.is_regular_file()) continue;
                // Skip environment files — already handled selectively above
                if (entry.path().extension() == ".environment") continue;
                fs::path rel = fs::relative(entry.path(), defsSrc, ec);
                fs::path dst = defsDst / rel;
                fs::create_directories(dst.parent_path(), ec);
                fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
                ec.clear();
            }
        }
    }

    // --- Copy prefabs directory (small text files, all potentially referenced) ---
    {
        fs::path prefabSrc = fs::path(resolveProjectPath("assets/prefabs"));
        fs::path prefabDst = outAssets / "prefabs";
        if (fs::exists(prefabSrc, ec)) {
            fs::copy(prefabSrc, prefabDst,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
            ec.clear();
        }
    }

    int fileCount = 0;
    uintmax_t totalBytes = 0;

    auto copyAsset = [&](const std::string& relPath) {
        try {
            fs::path dest = resourcesDir / relPath;
            fs::create_directories(dest.parent_path(), ec);
            std::string absPath = resolveProjectPath(relPath);
            if (fs::exists(absPath)) {
                fs::copy_file(absPath, dest, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    totalBytes += fs::file_size(absPath);
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
    for (auto& entry : fs::recursive_directory_iterator(appBundle, ec)) {
        if (entry.is_regular_file()) {
            packageSize += entry.file_size();
        }
    }

    double packageMB = static_cast<double>(packageSize) / (1024.0 * 1024.0);
    double assetMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);

    char summary[256];
    std::snprintf(summary, sizeof(summary),
                  "Package created: PixelRoguelike.app — %d referenced assets (%.1f MB assets, %.1f MB total)",
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
    return !std::filesystem::exists(resolveProjectPath(config.buildDir + "/CMakeCache.txt"));
}

// ---------------------------------------------------------------------------
// gameBinaryPath
// ---------------------------------------------------------------------------

std::string gameBinaryPath(const EditorBuildConfig& config) {
    return resolveProjectPath(config.buildDir + "/apps/runtime/pixel-roguelike");
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
        } else if (line.rfind("build_scene=", 0) == 0) {
            config.buildScenes.push_back(line.substr(12));
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
    for (const auto& scene : config.buildScenes) {
        file << "build_scene=" << scene << "\n";
    }
}
