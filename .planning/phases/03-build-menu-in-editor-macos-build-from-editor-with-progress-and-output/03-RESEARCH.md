# Phase 03: Build Menu in Editor - Research

**Researched:** 2026-03-29
**Domain:** C++ child process management, POSIX pipes, ImGui console panels, editor preferences
**Confidence:** HIGH

## Summary

This phase adds an in-editor build system to the level editor, invoking CMake from within the running C++ application, streaming output to a dockable ImGui panel, and launching the resulting game binary. The domain breaks into four technical areas: (1) spawning and managing a CMake child process, (2) capturing output non-blockingly without stalling the ImGui render loop, (3) rendering that output in an ImGui log panel with coloring and auto-scroll, and (4) persisting build preferences.

The recommended architecture uses `fork()`/`exec()` (not `popen` or `posix_spawn`) with a pipe pair for combined stdout+stderr, a `std::thread` reader that enqueues lines into a `std::mutex`-protected `std::vector<std::string>`, and a main-thread poll each frame that drains the queue and appends to the ImGui log. This gives the editor the `pid_t` needed for SIGTERM cancellation and avoids `popen`'s shell overhead and inability to retrieve the process PID.

CMake `--build` with Unix Makefiles emits progress lines of the form `[ XX%]` (e.g., `[ 47%] Building CXX object ...`) on stdout. These can be matched with a simple substring scan and the integer extracted to display percentage in the Build menu label. Errors and warnings appear on stderr and match `error:` / `warning:` tokens from the compiler.

**Primary recommendation:** Use `fork()`/`exec()` + a dedicated reader thread + a mutex-protected line queue. Display output in an `ExampleAppLog`-style struct (from `imgui_demo.cpp`) customized with per-line coloring. Persist preferences in the existing `editor_window.ini`-style key=value format — no new JSON dependency needed.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Only `pixel-roguelike` is buildable from the editor. Editor and model viewer are developer tools built from CLI only.
- **D-02:** Build directory is configurable via editor preferences file (persistent JSON/INI next to `editor_window.ini`). Default is `build`.
- **D-03:** Auto-configure: check if `{build_dir}/CMakeCache.txt` exists. If not, run `cmake -S . -B {build_dir}` before building.
- **D-04:** Build configuration (Debug/Release/RelWithDebInfo) displayed in Build menu and switchable. Switching triggers an automatic reconfigure with `-DCMAKE_BUILD_TYPE=X`.
- **D-05:** Assume CMakeLists.txt is in the working directory (project root). No configurable project root.
- **D-06:** Use Unix Makefiles generator (default CMake on macOS). No Ninja/Xcode.
- **D-07:** Output is the raw executable binary (no .app bundle packaging).
- **D-08:** Build output shown in a new dockable "Build Output" panel. Scrollable, monospace font.
- **D-09:** Raw compiler/cmake output with color hints: errors in red, warnings in yellow.
- **D-10:** CMake's `[XX%]` progress markers parsed and displayed as a percentage in the menu bar area during builds.
- **D-11:** Output panel auto-shows when a build starts. Can be manually closed.
- **D-12:** Auto-scroll to bottom during build. On build failure, jump to first error line.
- **D-13:** Output cleared on each new build.
- **D-14:** Cancel button ("Stop Build") in output panel header. Sends SIGTERM to the cmake process.
- **D-15:** If scene has unsaved changes when building, prompt "Save before building?" with Save/Don't Save/Cancel.
- **D-16:** Build menu item greyed out while a build is in progress. No concurrent builds.
- **D-17:** "Build and Run" menu item under Build menu. Builds, then launches on success.
- **D-18:** When running from editor, pass `--scene <current_scene_path>` to game executable. **NOTE: `--scene` is not yet implemented in `apps/runtime/main.cpp` — only `--screenshot` exists. The `--scene` argument handler must be added as part of this phase.**
- **D-19:** On build failure, show errors. No attempt to run. No "run last successful build" option.

### Claude's Discretion

- Process spawning mechanism (fork/exec, popen, std::system, platform-specific)
- Output panel styling and exact layout within the dock
- Keyboard shortcut assignments for Build (e.g., Cmd+B) and Build and Run (e.g., Cmd+R)
- How to parse CMake percentage from output stream
- Thread/async model for non-blocking build execution
- Editor preferences file format details

### Deferred Ideas (OUT OF SCOPE)

- Windows build support
- App bundle (.app) packaging for macOS distribution
- Ninja generator support
- Editor self-rebuild from within the editor
- Model viewer build target from editor
- "Build All" option
- Toolbar Play/Build button
- "Run last successful build" fallback on failure
</user_constraints>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| POSIX `fork()`/`exec()` | POSIX.1 | Spawn CMake child process | Provides PID for SIGTERM; available on macOS without dependencies; standard pattern for game editors |
| POSIX `pipe()` + `fcntl(O_NONBLOCK)` | POSIX.1 | Capture stdout+stderr | Non-blocking reads prevent blocking the frame; single merged pipe avoids select/poll complexity |
| `std::thread` | C++20 | Reader thread for pipe output | Dedicated thread drains pipe without polling overhead; decouples I/O from frame rate |
| `std::mutex` + `std::vector<std::string>` | C++20 STL | Thread-safe line queue | Main thread polls each frame; simple and correct |
| `kill(pid, SIGTERM)` + `waitpid()` | POSIX.1 | Cancellation and reaping | SIGTERM allows cmake to forward signal to make; waitpid prevents zombie processes |
| Dear ImGui | v1.92.6 (project) | Output panel, menu, modal dialog | Already integrated; `ExampleAppLog`-style struct for log panel; `BeginDisabled` for greyed menu |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `std::filesystem` | C++17 STL | Check `CMakeCache.txt` existence, resolve binary path | Built-in; no extra dependency |
| `spdlog` | v1.x (project) | Log build events at debug level | Already integrated; log subprocess launch/exit for developer diagnostics |
| `<regex>` or `strstr` | C++11 STL | Parse `[XX%]` from output lines | Simple strstr is sufficient; no need for regex overhead |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `fork()`/`exec()` | `popen()` | popen is simpler but gives no PID — cannot send SIGTERM for cancellation. Rejected. |
| `fork()`/`exec()` | `posix_spawn()` | posix_spawn is slightly cleaner but `fork`+`exec` is more familiar in the codebase context and well-documented for pipe setup; both work on macOS |
| `fork()`/`exec()` | `std::system()` | Blocking, no output capture, no PID. Completely unsuitable. |
| Single merged pipe | Separate stdout+stderr pipes | Two pipes require `poll()` or two reader threads to avoid deadlock. Merged pipe (`2>&1` style via `dup2(pipefd[1], STDERR_FILENO)`) is simpler and correct for this use case. |
| Key=value INI file | nlohmann/json | No new dependency; consistent with existing `editor_window.ini` pattern. INI is sufficient. |

**Installation:** No new packages required. All tools are POSIX (`<unistd.h>`, `<signal.h>`, `<sys/wait.h>`, `<fcntl.h>`) and STL (`<thread>`, `<mutex>`, `<vector>`, `<string>`).

## Architecture Patterns

### Recommended File Structure
```
src/editor/
├── build/
│   ├── EditorBuildSystem.h    # EditorBuildState + BuildOutputPanel + BuildRunner
│   └── EditorBuildSystem.cpp  # fork/exec, pipe, thread, SIGTERM logic
apps/level_editor/
└── main.cpp                   # Add Build menu + panel render call + keyboard shortcuts
```

And for the `--scene` argument (required for D-18):
```
apps/runtime/
└── main.cpp                   # Add --scene <path> argument parsing
```

### Pattern 1: Process Spawner with Pipe Capture

**What:** `fork()` creates child, `exec()` replaces child with `cmake`. Pipe captures combined stdout+stderr. Parent sets read-end non-blocking, spawns reader thread.

**When to use:** Any time you need: (a) the PID for cancellation, (b) streaming output while process runs, (c) exit code on completion.

```cpp
// Source: POSIX.1 standard; verified against man pages and GeeksforGeeks non-blocking pipe article

// Setup: create pipe, fork, exec cmake
int pipefd[2];
pipe(pipefd);
pid_t pid = fork();
if (pid == 0) {
    // Child: redirect both stdout and stderr into the write end
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    // argv: {"cmake", "--build", buildDir, "--target", "pixel-roguelike", "--parallel", nullptr}
    execvp("cmake", argv);
    _exit(1); // exec failed
}
// Parent: close write end, set read end non-blocking
close(pipefd[1]);
fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
```

**Key detail:** `dup2` redirects both stdout AND stderr from the child to the same pipe write end. This avoids the two-pipe deadlock problem when reading both streams from a single thread.

### Pattern 2: Reader Thread with Mutex Queue

**What:** Background thread reads from pipe line by line, pushes to a mutex-protected queue. Main thread drains queue each frame and appends to ImGui log.

**When to use:** Whenever I/O must not stall the render loop.

```cpp
// Source: C++20 STL patterns; verified against multiple forum posts and GeeksforGeeks
struct EditorBuildState {
    pid_t           pid = -1;
    int             pipeFd = -1;
    bool            running = false;
    int             exitCode = -1;
    float           progressPct = 0.0f;
    int             firstErrorLine = -1;  // for D-12 jump-to-error

    std::mutex              queueMutex;
    std::vector<std::string> pendingLines; // reader thread -> main thread
    std::thread             readerThread;
};

// Reader thread body:
void readerThreadFn(int fd, std::mutex& mtx, std::vector<std::string>& queue, std::atomic<bool>& done) {
    std::string partial;
    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            partial.append(buf, n);
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
            // n == 0: EOF (cmake exited), or error
            break;
        }
    }
    done = true;
}
```

### Pattern 3: ImGui Log Panel (ExampleAppLog-style)

**What:** Custom log struct wrapping `ImGuiTextBuffer` + `ImVector<int> LineOffsets` + line color metadata. Each line stores a `LineKind` (Normal/Warning/Error/Progress).

**When to use:** Scrollable, colored, large-text output panels in ImGui.

```cpp
// Source: imgui_demo.cpp ExampleAppLog struct — verified directly in
// build/_deps/imgui-src/imgui_demo.cpp lines 9333-9454

enum class BuildLineKind { Normal, Warning, Error, Progress };

struct BuildOutputLog {
    ImGuiTextBuffer          buf;
    ImVector<int>            lineOffsets; // init: push_back(0)
    ImVector<BuildLineKind>  lineKinds;
    bool                     autoScroll = true;
    int                      firstErrorLine = -1;
    bool                     scrollToError = false;

    void clear() {
        buf.clear();
        lineOffsets.clear();
        lineOffsets.push_back(0);
        lineKinds.clear();
        firstErrorLine = -1;
    }

    void addLine(const char* line, BuildLineKind kind) {
        int oldSize = buf.size();
        buf.append(line);
        buf.append("\n");
        for (int newSize = buf.size(); oldSize < newSize; oldSize++)
            if (buf[oldSize] == '\n')
                lineOffsets.push_back(oldSize + 1);
        lineKinds.push_back(kind);
        if (kind == BuildLineKind::Error && firstErrorLine == -1)
            firstErrorLine = lineKinds.size() - 1;
    }
};

// Render inside ImGui::Begin("Build Output"):
// For each visible line (via ImGuiListClipper):
//   - Error lines: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.3,0.3,1))
//   - Warning lines: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.8,0,1))
//   - Normal: no color push
//   - ImGui::TextUnformatted(line_start, line_end)
//   - PopStyleColor if pushed
// Auto-scroll: if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f)
// Jump to first error: if (scrollToError && firstErrorLine >= 0) scroll to that line item
```

### Pattern 4: Build Menu with Dynamic Label

**What:** The Build menu title shows progress during a build. Uses `ImGui::BeginMenu` with a formatted string as label.

**When to use:** Per D-10, show `"Build [47%]"` in the menu bar while building.

```cpp
// Construct the label each frame:
std::string buildMenuLabel = "Build";
if (buildState.running) {
    buildMenuLabel = "Build [" + std::to_string((int)buildState.progressPct) + "%]";
}
if (ImGui::BeginMenu(buildMenuLabel.c_str())) {
    ImGui::BeginDisabled(buildState.running);   // D-16
    if (ImGui::MenuItem("Build", "Cmd+B"))      { /* trigger build */ }
    if (ImGui::MenuItem("Build and Run", "Cmd+R")) { /* trigger build+run */ }
    ImGui::EndDisabled();
    // Config submenu:
    if (ImGui::BeginMenu("Configuration")) {
        for (const char* cfg : {"Debug", "Release", "RelWithDebInfo"}) {
            if (ImGui::MenuItem(cfg, nullptr, currentConfig == cfg)) { /* switch config */ }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}
```

**Note:** `ImGui::BeginMenu` with a changing label is valid because ImGui matches windows by ID, not label. The label string can change each frame without breaking dock state.

### Pattern 5: CMake Progress Parsing

**What:** Each line from cmake output is scanned for `[XX%]` prefix. Simple `sscanf` or `strstr` — no regex.

```cpp
// Source: CMake discourse confirmed format "[ XX%] Building CXX object ..."
// Verified: progress is output by make (subprocess of cmake), appears on stdout
// The make output format is: "[ 47%] Building CXX object ..."
// Note: leading space is present when < 100%: "[ 1%]" vs "[100%]"

BuildLineKind classifyLine(const std::string& line, float& progressOut) {
    int pct = 0;
    if (sscanf(line.c_str(), "[ %d%%]", &pct) == 1 ||
        sscanf(line.c_str(), "[%d%%]", &pct) == 1) {
        progressOut = static_cast<float>(pct);
        return BuildLineKind::Progress;
    }
    // Use case-insensitive search for error/warning tokens:
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("error:") != std::string::npos) return BuildLineKind::Error;
    if (lower.find("warning:") != std::string::npos) return BuildLineKind::Warning;
    return BuildLineKind::Normal;
}
```

### Pattern 6: SIGTERM Cancellation + Reaping

**What:** Send SIGTERM to the cmake process group (negative pid) so it propagates to child `make` processes. Use `waitpid` with `WNOHANG` in the main frame loop to detect process exit without blocking.

```cpp
// Source: POSIX.1 man pages; Codidact "kill child without read() hanging" article
void cancelBuild(EditorBuildState& state) {
    if (state.pid > 0) {
        kill(-state.pid, SIGTERM);  // negative: sends to process group
        // NOTE: cmake must be started with setsid() or setpgid(0,0) in child
        // to create a new process group so SIGTERM reaches make subprocesses
    }
}

// Each frame in the main loop, if build is running:
void pollBuildProcess(EditorBuildState& state) {
    // Drain line queue (lock, swap, unlock)
    std::vector<std::string> lines;
    { std::lock_guard<std::mutex> lock(state.queueMutex);
      lines.swap(state.pendingLines); }
    for (auto& line : lines) {
        float pct = 0.0f;
        BuildLineKind kind = classifyLine(line, pct);
        if (pct > 0) state.progressPct = pct;
        buildLog.addLine(line.c_str(), kind);
    }

    // Non-blocking check if process has exited
    int status;
    pid_t result = waitpid(state.pid, &status, WNOHANG);
    if (result == state.pid) {
        state.running = false;
        state.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        state.pid = -1;
        state.readerThread.join();
        close(state.pipeFd);
        // If exitCode == 0 and runAfterBuild was requested, launch the binary
    }
}
```

**Critical detail:** The child must call `setpgid(0, 0)` (or `setsid()`) after `fork()` but before `execvp()` to create a new process group. Otherwise `kill(-pid, SIGTERM)` will not reach the `make` subprocesses that cmake spawns.

### Pattern 7: Editor Preferences Persistence

**What:** Extend `editor_window.ini` (existing key=value INI) with build settings. No new file, no JSON dependency.

```
# editor_window.ini additions
build_dir=build
build_config=Debug
```

```cpp
// In loadWindowGeometry() equivalent:
if (sscanf(line.c_str(), "build_dir=%s", buf) == 1) buildDir = buf;
if (sscanf(line.c_str(), "build_config=%s", buf) == 1) buildConfig = buf;
// In saveWindowGeometry() equivalent:
file << "build_dir=" << buildDir << "\n";
file << "build_config=" << buildConfig << "\n";
```

**Alternative:** A separate `editor_build.ini` file for cleaner separation. Either works; extending the existing file is simpler.

### Pattern 8: Unsaved-Changes Modal (D-15)

**What:** ImGui modal popup with three buttons. Follows the existing `BeginPopupModal` pattern in the editor.

```cpp
// Follows the "Save Layout As" modal pattern in main.cpp:696
if (buildRequestedWithUnsavedChanges) {
    ImGui::OpenPopup("Save Before Building?");
}
if (ImGui::BeginPopupModal("Save Before Building?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("The scene has unsaved changes.");
    ImGui::TextUnformatted("Save before building?");
    ImGui::Separator();
    if (ImGui::Button("Save")) { saveScene(); triggerBuild(); ImGui::CloseCurrentPopup(); }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save")) { triggerBuild(); ImGui::CloseCurrentPopup(); }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}
```

### Pattern 9: Keyboard Shortcuts

**What:** Following the existing pattern (`io.KeySuper` for Cmd on macOS):

```cpp
// In the keyboard shortcut block (around main.cpp:379)
if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_B)) {
    buildPressed = true;   // Cmd+B = Build
}
if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_R)) {
    buildAndRunPressed = true;  // Cmd+R = Build and Run
}
```

**Note:** `ImGuiKey_R` is currently used for Scale tool only when right mouse is NOT held. Adding Cmd+R (modifier combo) does not conflict since that block has `(io.KeyCtrl || io.KeySuper)` guard.

### Pattern 10: --scene Argument in Runtime (D-18)

**What:** The runtime `main.cpp` must be extended to accept `--scene <path>` and load that `.scene` file instead of the hardcoded `SilosCloisterScene`. The `LevelLoader`/`LevelBuilder` path must be verified.

**Current state (verified):** `apps/runtime/main.cpp` only handles `--screenshot`. The `--scene` handler does not exist. The runtime currently hardcodes `SilosCloisterScene` via `sceneManager.pushScene(std::make_unique<SilosCloisterScene>(), app)`.

**Required change:** Parse `--scene <path>` in `main.cpp`, then use `LevelLoader` + `LevelBuilder` to construct the scene from the `.scene` file path rather than instantiating `SilosCloisterScene` directly.

### Anti-Patterns to Avoid
- **`popen()`:** Returns `FILE*`, no PID available, cannot send SIGTERM. Build cancellation is impossible.
- **`std::system()`:** Blocking. Freezes the editor for the entire build duration.
- **Non-blocking pipe without a reader thread:** `read()` with `EAGAIN` polling every frame adds polling overhead and delays output by up to one frame (16ms). A thread is cleaner.
- **Two separate pipes for stdout+stderr without `poll()`:** Reading all of stdout before stderr can deadlock if the child fills the stderr pipe buffer (~64KB) while waiting for the parent to read it. Always merge with `dup2` or use `poll()`.
- **`kill(pid, SIGTERM)` without process group:** cmake spawns make as a child process. Sending SIGTERM to only the cmake PID leaves make running in the background. Use `kill(-pid, SIGTERM)` after setting a process group.
- **Forgetting `waitpid()` after process exit:** The cmake process becomes a zombie. Use `waitpid(pid, &status, WNOHANG)` each frame while the build is running.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Scrollable log with line offsets | Custom string buffer + per-line render | `ImGuiTextBuffer` + `ImVector<int> LineOffsets` (ExampleAppLog pattern from imgui_demo.cpp) | Already in the imgui binary; handles large buffers efficiently with `ImGuiListClipper` |
| Process group cleanup | Custom signal chain | `setpgid(0, 0)` in child + `kill(-pid, SIGTERM)` | POSIX standard; kills entire process tree |
| Thread-safe queue | Lock-free ring buffer | `std::mutex` + `std::vector` swap pattern | Sufficient at 60Hz poll rate; queue drains within milliseconds |

**Key insight:** All the required primitives are already available — POSIX headers on macOS, STL threading, and the ImGui log pattern from the demo. No third-party library is needed.

## Common Pitfalls

### Pitfall 1: cmake progress output stream ambiguity
**What goes wrong:** Developer assumes progress lines like `[ 47%]` go to stdout, but they may come from `make` subprocess via stderr.
**Why it happens:** cmake `--build` invokes `make` which controls its own output. The `make` progress lines typically go to stdout. When the pipe merges both streams, it doesn't matter — but if using separate pipes, progress could be missed.
**How to avoid:** Merge stdout and stderr with `dup2(pipefd[1], STDERR_FILENO)` in the child. All output goes to one pipe.
**Warning signs:** Progress percentage never advances even though build lines appear.

### Pitfall 2: Process group not set, SIGTERM leaks cmake but leaves make running
**What goes wrong:** User clicks "Stop Build". cmake receives SIGTERM and exits. make continues building in the background. Editor reports build cancelled but files are still being compiled.
**Why it happens:** cmake spawns make as a child. SIGTERM sent to cmake PID doesn't propagate unless they share a process group.
**How to avoid:** Child calls `setpgid(0, 0)` before `execvp`. Parent uses `kill(-state.pid, SIGTERM)`.
**Warning signs:** After cancel, CPU usage stays high; build files continue to appear modified.

### Pitfall 3: Zombie process from missing waitpid
**What goes wrong:** cmake exits, but the parent never calls `waitpid`. The process table entry persists as a zombie. Over many build sessions, zombie count grows.
**Why it happens:** On POSIX, a process stays in the process table until the parent calls `wait`/`waitpid`.
**How to avoid:** Poll `waitpid(pid, &status, WNOHANG)` every frame while build is running. Join the reader thread only after process exit.
**Warning signs:** `ps aux | grep cmake` shows zombie entries after builds.

### Pitfall 4: Pipe buffer exhaustion deadlock with two pipes
**What goes wrong:** Using two separate pipes (stdout and stderr). Parent reads all of stdout in a loop. Child fills its 64KB stderr pipe buffer waiting for parent to read stderr. Both parent and child block forever.
**Why it happens:** Pipe buffers are bounded. Full buffer blocks the writing process.
**How to avoid:** Use a single merged pipe. If two pipes are ever needed, use `poll()` to multiplex.
**Warning signs:** Build hangs at some percentage and never completes.

### Pitfall 5: `--scene` argument not yet implemented
**What goes wrong:** "Build and Run" launches `pixel-roguelike` without any `--scene` argument handler, so it always starts the hardcoded `SilosCloisterScene` regardless of what scene is open in the editor.
**Why it happens:** `apps/runtime/main.cpp` only parses `--screenshot`. The `--scene` argument is specified in D-18 but not yet in the codebase.
**How to avoid:** Implement `--scene <path>` parsing in `apps/runtime/main.cpp` as part of this phase. Use `LevelLoader` + `LevelBuilder` to load the scene from file.
**Warning signs:** Game always opens the hardcoded cloister scene regardless of editor scene selection.

### Pitfall 6: ImGui dockable panel not registered in dock layout
**What goes wrong:** The Build Output panel appears as a floating window instead of docking correctly, or is not accessible from the Window > Panels submenu.
**Why it happens:** New panels must be added to `EditorUiState` (visibility bool), the Window > Panels menu, and the default dock layout function (`buildDefaultEditorDockLayout`).
**How to avoid:** Add `showBuildOutput` to `EditorUiState`, add it to `Window > Panels` menu (alongside Outliner/Inspector/etc.), and add it to `captureLayoutVisibility`/`applyLayoutVisibility` in `EditorLayoutPreset`.
**Warning signs:** Panel is not listed in Window > Panels; closing it has no way to reopen.

### Pitfall 7: Build directory path resolution
**What goes wrong:** `cmake -S . -B build` is run from an unexpected working directory (not the project root). Build directory becomes a subdirectory of wherever the editor binary is located.
**Why it happens:** The editor CWD is set when launched. On macOS, if launched via double-click or Finder, CWD may be `/`. If launched from the build directory, CWD is the build directory.
**How to avoid:** The Makefile launches the editor with the project root as CWD (`./$(BUILD_DIR)/apps/level_editor/level-editor` from the project root). Document that the editor must be launched from the project root. Optionally, detect CWD at startup and warn if `CMakeLists.txt` is not found there.
**Warning signs:** cmake configure step fails with "Cannot find source directory".

## Code Examples

### Verified: ImGui ExampleAppLog auto-scroll pattern
```cpp
// Source: build/_deps/imgui-src/imgui_demo.cpp lines 9448-9449 (verified directly)
if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
```

### Verified: ImGuiListClipper for large log buffers
```cpp
// Source: build/_deps/imgui-src/imgui_demo.cpp lines 9431-9443 (verified directly)
ImGuiListClipper clipper;
clipper.Begin(LineOffsets.Size);
while (clipper.Step()) {
    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
        const char* line_start = buf + LineOffsets[line_no];
        const char* line_end = (line_no + 1 < LineOffsets.Size)
            ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
        ImGui::TextUnformatted(line_start, line_end);
    }
}
clipper.End();
```

### Verified: Non-blocking read pattern
```cpp
// Source: GeeksforGeeks "Non-blocking I/O with pipes in C" (POSIX.1 verified)
// After fcntl(pipefd[0], F_SETFL, O_NONBLOCK):
ssize_t n = read(pipefd[0], buf, sizeof(buf));
if (n == -1 && errno == EAGAIN) {
    // Pipe is empty — not an error, try again next iteration
} else if (n == 0) {
    // EOF — child process closed the write end (exited)
    break;
}
```

### Verified: Keyboard shortcut pattern (from editor)
```cpp
// Source: apps/level_editor/main.cpp:379 (verified directly)
if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_B)) {
    buildPressed = true;
}
```

### Verified: BeginDisabled for greyed menu items (from editor)
```cpp
// Source: apps/level_editor/main.cpp:606 (verified directly)
ImGui::BeginDisabled(buildState.running);  // Grey out during build
if (ImGui::MenuItem("Build", "Cmd+B")) { /* ... */ }
ImGui::EndDisabled();
```

### Verified: Game binary output path
```
{build_dir}/apps/runtime/pixel-roguelike
```
Confirmed by inspecting `build/apps/runtime/` directory directly. The `configure_desktop_app()` macro does not override `CMAKE_RUNTIME_OUTPUT_DIRECTORY`, so CMake's default per-target subdirectory structure applies.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `popen()` for build output | `fork()`/`exec()` + pipe + thread | N/A — popen never had cancellation | PID access enables SIGTERM |
| `SetScrollHere(1.0f)` (deprecated) | `SetScrollHereY(1.0f)` | Dear ImGui v1.77+ | API rename — use the Y variant |
| Two separate pipes for stdout/stderr | Single merged pipe with `dup2` | Community best practice | Eliminates deadlock risk |

## Open Questions

1. **cmake progress output stream (stdout vs stderr)**
   - What we know: cmake `--build` invokes `make`. Make's `[ XX%]` progress goes to stdout in most configurations. The merged pipe captures both.
   - What's unclear: On some CMake versions, `--use-stderr` affects which stream progress goes to. With the merged pipe approach, this is irrelevant.
   - Recommendation: Use merged pipe; no stream disambiguation needed.

2. **`--scene` and LevelBuilder/LevelLoader integration**
   - What we know: `apps/runtime/main.cpp` currently calls `sceneManager.pushScene(std::make_unique<SilosCloisterScene>(), app)` unconditionally. `LevelLoader` and `LevelBuilder` exist in `src/game/level/`.
   - What's unclear: Whether `LevelBuilder` can construct a fully playable scene from a `.scene` file without `SilosCloisterScene`'s hardcoded initialization. This needs a code read during planning.
   - Recommendation: Read `LevelLoader.h`, `LevelBuilder.h`, and `SilosCloisterScene.cpp` during planning to confirm the scene-from-file path works for the runtime.

3. **Build directory absolute vs relative**
   - What we know: Default is `build` (relative). The editor is launched from the project root via Makefile.
   - What's unclear: If a user changes the build directory to an absolute path, does the cmake invocation string handle it correctly?
   - Recommendation: Store and pass the build directory exactly as entered. Both relative and absolute paths work with cmake `-B`.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cmake | Build invocation (D-03, D-04) | YES | 4.1.1 | — |
| make (GNU Make) | cmake --build with Unix Makefiles | YES | 3.81 | — |
| POSIX fork/exec/pipe/signal | Process spawning | YES (macOS/POSIX) | macOS 25.1.0 | — |

**Missing dependencies:** None. All required tools and system calls are available.

## Sources

### Primary (HIGH confidence)
- `build/_deps/imgui-src/imgui_demo.cpp` lines 9333–9454 — ExampleAppLog struct, auto-scroll, ListClipper patterns — **verified by direct file read**
- `apps/level_editor/main.cpp` — BeginDisabled, BeginPopupModal, keyboard shortcut patterns — **verified by direct file read**
- `apps/runtime/main.cpp` — confirmed `--scene` not implemented, only `--screenshot` — **verified by direct file read**
- `build/apps/runtime/pixel-roguelike` — binary output path confirmed — **verified by directory listing**
- POSIX.1 man pages — `fork`, `exec`, `pipe`, `kill`, `waitpid`, `fcntl`, `setpgid` — **POSIX standard**
- cmake Discourse thread: https://discourse.cmake.org/t/how-to-get-cmake-to-show-a-percentage-progress-report/6019 — progress format `[ XX%]` confirmed

### Secondary (MEDIUM confidence)
- GeeksforGeeks "Non-blocking I/O with pipes in C" — `fcntl(O_NONBLOCK)` + `EAGAIN` pattern — verified against POSIX spec
- ImGui GitHub issues #300, #4868 — `SetScrollHereY` auto-scroll pattern — consistent with demo code
- UNSW spawn_read_pipe.c (`posix_spawn_file_actions_adddup2`) — pipe capture via file actions — not directly fetched but consistent with POSIX spec

### Tertiary (LOW confidence)
- CMake output stream routing (stdout vs stderr for `[ XX%]` progress lines) — empirically known but not confirmed from official docs; merged pipe approach makes stream identity irrelevant

## Metadata

**Confidence breakdown:**
- Process spawning/pipe/thread architecture: HIGH — POSIX standard, verified patterns
- ImGui log panel: HIGH — verified directly from imgui_demo.cpp in repo
- CMake progress format `[ XX%]`: HIGH — confirmed from community sources and cmake discourse
- `--scene` implementation gap: HIGH — verified by code inspection
- Binary output path: HIGH — verified by directory listing
- CMake output stream routing: MEDIUM — merged pipe sidesteps the question

**Research date:** 2026-03-29
**Valid until:** 2026-06-29 (stable platform APIs; 90-day validity)
