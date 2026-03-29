#pragma once

#include <imgui.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/types.h>

// ---------------------------------------------------------------------------
// BuildLineKind — per-line output classification
// ---------------------------------------------------------------------------

enum class BuildLineKind { Normal, Warning, Error, Progress };

// ---------------------------------------------------------------------------
// BuildOutputLog — ImGui ExampleAppLog-style buffer with per-line metadata
// ---------------------------------------------------------------------------

struct BuildOutputLog {
    ImGuiTextBuffer         buf;
    ImVector<int>           lineOffsets;  // byte offset of each line start; init: push_back(0)
    ImVector<BuildLineKind> lineKinds;
    bool                    autoScroll    = true;
    int                     firstErrorLine = -1;  // index of first Error line (D-12)
    bool                    scrollToError  = false;

    BuildOutputLog() { lineOffsets.push_back(0); }

    void clear() {
        buf.clear();
        lineOffsets.clear();
        lineOffsets.push_back(0);
        lineKinds.clear();
        firstErrorLine = -1;
        scrollToError  = false;
    }

    void addLine(const char* line, BuildLineKind kind) {
        int oldSize = buf.size();
        buf.append(line);
        buf.append("\n");
        for (int newSize = buf.size(); oldSize < newSize; ++oldSize)
            if (buf[oldSize] == '\n')
                lineOffsets.push_back(oldSize + 1);
        lineKinds.push_back(kind);
        if (kind == BuildLineKind::Error && firstErrorLine == -1)
            firstErrorLine = lineKinds.Size - 1;
    }

    int lineCount() const { return lineOffsets.Size; }
};

// ---------------------------------------------------------------------------
// EditorBuildConfig — persistent build preferences (D-02, D-04)
// ---------------------------------------------------------------------------

struct EditorBuildConfig {
    std::string buildDir    = "build";  // configurable build directory
    std::string buildConfig = "Debug";  // Debug / Release / RelWithDebInfo
};

// ---------------------------------------------------------------------------
// EditorBuildState — runtime state of a running (or idle) build
// ---------------------------------------------------------------------------

struct EditorBuildState {
    pid_t  pid     = -1;
    int    pipeFd  = -1;
    bool   running = false;
    bool   runAfterBuild = false;  // true when triggered by "Build and Run" (D-17)
    int    exitCode = -1;
    float  progressPct = 0.0f;
    std::string currentScenePath;  // --scene path for game launch (D-18)

    std::mutex               queueMutex;
    std::vector<std::string> pendingLines;  // reader thread -> main thread
    std::thread              readerThread;
    std::atomic<bool>        readerDone{false};
};

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

/// Classify a raw output line. Sets progressOut if kind == Progress.
BuildLineKind classifyLine(const std::string& line, float& progressOut);

/// Start a cmake --build invocation for the given target (non-blocking).
void startBuild(EditorBuildState& state, const EditorBuildConfig& config, const std::string& target);

/// Start cmake -S . -B {buildDir} -DCMAKE_BUILD_TYPE=X (auto-configure, D-03/D-04).
void startConfigure(EditorBuildState& state, const EditorBuildConfig& config);

/// Send SIGTERM to the cmake process group (D-14).
void cancelBuild(EditorBuildState& state);

/// Drain the line queue and check waitpid; call once per frame while running.
void pollBuild(EditorBuildState& state, BuildOutputLog& log);

/// Returns true if {buildDir}/CMakeCache.txt does not exist (D-03).
bool needsConfigure(const EditorBuildConfig& config);

/// Returns the expected path to the game binary.
std::string gameBinaryPath(const EditorBuildConfig& config);

/// Launch the game binary (fire-and-forget fork+exec, D-17/D-18).
void launchGame(const EditorBuildConfig& config, const std::string& scenePath);

/// Load build preferences from a key=value INI file.
void loadBuildConfig(EditorBuildConfig& config, const std::string& path);

/// Save build preferences to a key=value INI file.
void saveBuildConfig(const EditorBuildConfig& config, const std::string& path);
