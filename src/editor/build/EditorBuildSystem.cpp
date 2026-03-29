#include "editor/build/EditorBuildSystem.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include <spdlog/spdlog.h>

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
