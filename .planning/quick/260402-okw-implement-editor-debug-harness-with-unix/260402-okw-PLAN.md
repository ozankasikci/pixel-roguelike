---
phase: quick
plan: 260402-okw
type: execute
wave: 1
depends_on: []
files_modified:
  - CMakeLists.txt
  - src/editor/CMakeLists.txt
  - src/editor/debug/CommandRegistry.h
  - src/editor/debug/CommandRegistry.cpp
  - src/editor/debug/DebugServer.h
  - src/editor/debug/DebugServer.cpp
  - src/editor/debug/EditorInspector.h
  - src/editor/debug/EditorInspector.cpp
  - src/editor/debug/EditorCommander.h
  - src/editor/debug/EditorCommander.cpp
  - src/editor/debug/SessionRecorder.h
  - src/editor/debug/SessionRecorder.cpp
  - src/editor/debug/SessionPlayer.h
  - src/editor/debug/SessionPlayer.cpp
  - src/editor/debug/DebugHarness.h
  - src/editor/debug/DebugHarness.cpp
  - apps/level_editor/main.cpp
  - tools/editor-ctl.py
  - .gitignore
autonomous: true

must_haves:
  truths:
    - "External process can connect to the running editor via Unix socket and send JSON commands"
    - "inspect.* commands return correct read-only editor state (selection, gizmo, panels, undo stack, frame stats)"
    - "command.* commands mutate editor state (select, deselect, set gizmo, undo, redo)"
    - "record.start captures timestamped events, record.stop writes .editsession JSON"
    - "editor-ctl.py CLI tool can send commands and print responses"
    - "Debug harness has zero impact on game executable — only linked into level-editor"
  artifacts:
    - path: "src/editor/debug/DebugHarness.h"
      provides: "Top-level facade for debug harness lifecycle"
    - path: "src/editor/debug/DebugServer.h"
      provides: "Unix socket listener with JSON parse/dispatch"
    - path: "src/editor/debug/CommandRegistry.h"
      provides: "String-to-handler command routing"
    - path: "src/editor/debug/EditorInspector.h"
      provides: "Read-only state queries"
    - path: "src/editor/debug/EditorCommander.h"
      provides: "Mutating editor commands"
    - path: "src/editor/debug/SessionRecorder.h"
      provides: "Event capture and serialization"
    - path: "src/editor/debug/SessionPlayer.h"
      provides: "Replay from .editsession files"
    - path: "tools/editor-ctl.py"
      provides: "CLI wrapper for socket communication"
  key_links:
    - from: "apps/level_editor/main.cpp"
      to: "src/editor/debug/DebugHarness.h"
      via: "DebugHarness created after editor init, poll() called per frame"
    - from: "src/editor/debug/DebugServer.cpp"
      to: "src/editor/debug/CommandRegistry.h"
      via: "Server parses JSON, dispatches cmd string through registry"
    - from: "src/editor/debug/CommandRegistry.cpp"
      to: "src/editor/debug/EditorInspector.h"
      via: "inspect.* commands routed to inspector methods"
    - from: "src/editor/debug/CommandRegistry.cpp"
      to: "src/editor/debug/EditorCommander.h"
      via: "command.* commands routed to commander methods"
---

<objective>
Implement a remote control/debug harness for the level editor using Unix domain sockets and newline-delimited JSON protocol.

Purpose: Allow external tools (Claude Code, Python scripts, test harnesses) to programmatically inspect and control the running editor for debugging, automated testing, and session recording.

Output: Complete debug harness system in src/editor/debug/ with socket server, command routing, inspector, commander, session recorder/player, main.cpp integration, and CLI tool.
</objective>

<execution_context>
@docs/plans/2026-04-02-editor-debug-harness-design.md
</execution_context>

<context>
@docs/plans/2026-04-02-editor-debug-harness-design.md (Full design document — protocol, commands, recording format)
@src/editor/scene/EditorSceneDocument.h (Document API — objects(), findObject(), captureState())
@src/editor/ui/LevelEditorUi.h (EditorUiState — tool, panel visibility, selectedIds pattern)
@src/editor/core/EditorCommand.h (EditorCommandStack — canUndo/canRedo, undo/redo, undoLabel/redoLabel)
@src/editor/viewport/EditorViewportController.h (EditorTransformTool, EditorCamera, editorGizmoIsHot())
@apps/level_editor/main.cpp (Integration point — init after line ~318, poll in render lambda, shutdown before imgui.shutdown())
@src/editor/CMakeLists.txt (Add debug/*.cpp sources here)
@CMakeLists.txt (Add nlohmann/json FetchContent here)

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/editor/scene/EditorSceneDocument.h:
```cpp
class EditorSceneDocument {
    const std::vector<EditorSceneObject>& objects() const;
    EditorSceneObject* findObject(std::uint64_t id);
    std::string scenePath() const;
    EditorSceneDocumentState captureState();
    void restoreState(const EditorSceneDocumentState& state);
    bool sceneDirty() const;
};
std::string editorSceneObjectLabel(const EditorSceneObject& object);
```

From src/editor/core/EditorCommand.h:
```cpp
class EditorCommandStack {
    bool canUndo() const;
    bool canRedo() const;
    const char* undoLabel() const;
    const char* redoLabel() const;
    bool undo(EditorSceneDocument& document);
    bool redo(EditorSceneDocument& document);
};
```

From src/editor/ui/LevelEditorUi.h:
```cpp
struct EditorUiState {
    EditorTransformTool tool;
    bool showOutliner, showInspector, showAssetBrowser, showEnvironment, showViewport, showBuildOutput;
    // ... other fields
};
```

From src/editor/viewport/EditorViewportController.h:
```cpp
enum class EditorTransformTool { Translate, Rotate, Scale };
bool editorGizmoIsHot();
```

From apps/level_editor/main.cpp (local variables the harness needs access to):
```cpp
EditorSceneDocument document;        // line 292
EditorCommandStack commandStack;     // line 300
std::vector<std::uint64_t> selectedIds; // line 333
EditorUiState ui;                    // implied from ui.tool, ui.showOutliner, etc.
Window window;                       // line ~265
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add nlohmann/json dependency and create debug harness core (server, registry, inspector, commander)</name>
  <files>
    CMakeLists.txt,
    src/editor/CMakeLists.txt,
    src/editor/debug/CommandRegistry.h,
    src/editor/debug/CommandRegistry.cpp,
    src/editor/debug/DebugServer.h,
    src/editor/debug/DebugServer.cpp,
    src/editor/debug/EditorInspector.h,
    src/editor/debug/EditorInspector.cpp,
    src/editor/debug/EditorCommander.h,
    src/editor/debug/EditorCommander.cpp,
    src/editor/debug/DebugHarness.h,
    src/editor/debug/DebugHarness.cpp
  </files>
  <action>
    **1. Add nlohmann/json via FetchContent** in the top-level CMakeLists.txt (before `enable_testing()`):
    ```cmake
    # nlohmann/json v3.11.3 - JSON parsing for editor debug harness
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_PROGRESS   TRUE
    )
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
    ```

    **2. Update src/editor/CMakeLists.txt** to add all debug/*.cpp files to the `editor` static library source list and link `nlohmann_json::nlohmann_json`:
    ```cmake
    add_library(editor STATIC
        # ... existing sources ...
        debug/CommandRegistry.cpp
        debug/DebugHarness.cpp
        debug/DebugServer.cpp
        debug/EditorCommander.cpp
        debug/EditorInspector.cpp
        debug/SessionRecorder.cpp
        debug/SessionPlayer.cpp
    )
    # ... existing target_include_directories ...
    target_link_libraries(editor PUBLIC gameplay nlohmann_json::nlohmann_json)
    ```

    **3. Create CommandRegistry** (`src/editor/debug/CommandRegistry.h/.cpp`):
    - Type alias: `using CommandHandler = std::function<nlohmann::json(const nlohmann::json& args)>`
    - `void registerCommand(const std::string& cmd, CommandHandler handler)` — stores in `std::unordered_map<std::string, CommandHandler>`
    - `nlohmann::json dispatch(const std::string& cmd, const nlohmann::json& args)` — looks up handler, returns `{"ok": true, "data": ...}` on success, `{"ok": false, "error": "Unknown command: ..."}` on miss
    - No inheritance, simple value type

    **4. Create DebugServer** (`src/editor/debug/DebugServer.h/.cpp`):
    - POSIX Unix domain socket at `/tmp/pixel-roguelike-editor-{pid}.sock`
    - `void init()` — creates socket, bind, listen (backlog 2), sets O_NONBLOCK on listen fd
    - `void shutdown()` — closes all client fds, closes listen fd, `unlink()` the socket path
    - `void poll(CommandRegistry& registry)` — non-blocking:
      1. `accept()` new connections (non-blocking), add to client list, set O_NONBLOCK on client fd
      2. For each connected client: `recv()` into per-client read buffer
      3. Split buffer on `\n`, parse each complete line as JSON
      4. Extract `id`, `cmd`, `args` fields from request JSON
      5. Call `registry.dispatch(cmd, args)` to get response JSON
      6. Attach the request `id` to the response, serialize + `\n`, `send()` back
      7. On recv error or client disconnect, remove from client list and close fd
    - Store client fds in a `std::vector<ClientConnection>` where `ClientConnection` has fd + read buffer string
    - Guard all POSIX calls with `#if defined(__unix__) || defined(__APPLE__)` — on Windows, init() logs a warning and returns without opening a socket
    - Use `<sys/socket.h>`, `<sys/un.h>`, `<unistd.h>`, `<fcntl.h>`, `<cerrno>`
    - Socket path stored as member, constructed in init() from `getpid()`

    **5. Create EditorInspector** (`src/editor/debug/EditorInspector.h/.cpp`):
    - Constructor takes const references/pointers to the editor state it needs to read:
      `EditorInspector(const EditorSceneDocument& doc, const std::vector<std::uint64_t>& selectedIds, const EditorUiState& ui, const EditorCommandStack& cmdStack)`
    - Methods returning `nlohmann::json`:
      - `selection()` — returns array of selected entity labels (use `editorSceneObjectLabel()`), gizmo mode string, gizmo_active from `editorGizmoIsHot()`
      - `gizmoState()` — current tool as string ("Translate"/"Rotate"/"Scale"), active (dragging) from `editorGizmoIsHot()`
      - `inputState()` — mouse position from `ImGui::GetMousePos()`, button states from `ImGui::IsMouseDown()`, modifier keys from `ImGui::GetIO().KeyCtrl/KeyShift/KeyAlt`
      - `frameStats()` — fps from `ImGui::GetIO().Framerate`, delta from `ImGui::GetIO().DeltaTime`
      - `undoStack()` — canUndo, canRedo, undoLabel, redoLabel from EditorCommandStack
      - `panels()` — map of panel name to visible bool from EditorUiState (showOutliner, showInspector, showAssetBrowser, showEnvironment, showViewport, showBuildOutput)
    - All methods are const, no mutations

    **6. Create EditorCommander** (`src/editor/debug/EditorCommander.h/.cpp`):
    - Constructor takes mutable references:
      `EditorCommander(EditorSceneDocument& doc, std::vector<std::uint64_t>& selectedIds, EditorUiState& ui, EditorCommandStack& cmdStack)`
    - Methods returning `nlohmann::json` (all return `{"ok": true}` or error):
      - `selectEntity(const std::string& name)` — iterate doc.objects(), find by `editorSceneObjectLabel()` match, set selectedIds to just that entity's id. Return error if not found.
      - `deselectAll()` — clear selectedIds
      - `setGizmo(const std::string& mode)` — parse "Translate"/"Rotate"/"Scale" to EditorTransformTool, set ui.tool. Error on unknown mode.
      - `undo()` — call cmdStack.undo(doc), return ok/error based on canUndo
      - `redo()` — call cmdStack.redo(doc), return ok/error based on canRedo
      - `togglePanel(const std::string& panel)` — map panel name to the corresponding ui.show* bool and toggle it. Support: "outliner", "inspector", "asset_browser", "environment", "viewport", "build_output". Error on unknown panel.
    - Note: key_press, mouse_click, and drag commands from the design doc are deferred to a follow-up — they require injecting GLFW events which is complex. The commander registers these command names but returns `{"ok": false, "error": "Not yet implemented"}`.

    **7. Create DebugHarness** (`src/editor/debug/DebugHarness.h/.cpp`):
    - Facade that owns DebugServer, CommandRegistry, EditorInspector, EditorCommander, SessionRecorder, SessionPlayer
    - Constructor: `DebugHarness(EditorSceneDocument& doc, std::vector<std::uint64_t>& selectedIds, EditorUiState& ui, EditorCommandStack& cmdStack)`
    - `void init()`:
      1. Create inspector and commander with the editor references
      2. Register all inspect.* commands in registry, routing to inspector methods
      3. Register all command.* commands in registry, routing to commander methods
      4. Register record.* and replay.* commands (routing to recorder/player, created in Task 2)
      5. Call server.init()
      6. Log socket path via spdlog::info
    - `void poll()` — calls server.poll(registry). Also calls sessionPlayer.tick() if replay is active.
    - `void shutdown()` — calls server.shutdown(), recorder.stop() if recording
    - Non-copyable, non-movable (raw fd ownership via DebugServer)
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target editor 2>&1 | tail -20</automated>
  </verify>
  <done>
    All 7 source files in src/editor/debug/ compile as part of the editor static library. nlohmann/json fetched via FetchContent. CommandRegistry dispatches string commands to handlers. DebugServer opens a Unix socket and does non-blocking poll. EditorInspector reads editor state. EditorCommander mutates editor state. DebugHarness ties them together.
  </done>
</task>

<task type="auto">
  <name>Task 2: Implement session recorder/player and integrate harness into main.cpp</name>
  <files>
    src/editor/debug/SessionRecorder.h,
    src/editor/debug/SessionRecorder.cpp,
    src/editor/debug/SessionPlayer.h,
    src/editor/debug/SessionPlayer.cpp,
    apps/level_editor/main.cpp,
    .gitignore
  </files>
  <action>
    **1. Create SessionRecorder** (`src/editor/debug/SessionRecorder.h/.cpp`):
    - Holds: `bool recording_`, `std::string filePath_`, `std::vector<nlohmann::json> events_`, `std::chrono::steady_clock::time_point startTime_`
    - `nlohmann::json start(const std::string& file, const std::string& scenePath)`:
      - If file is empty, generate default path: `assets/debug-sessions/{timestamp}.editsession`
      - Create `assets/debug-sessions/` directory if needed (`std::filesystem::create_directories`)
      - Set recording_ = true, clear events_, record startTime_
      - Push initial snapshot event at t=0
      - Return `{"ok": true, "file": filePath_}`
    - `nlohmann::json stop()`:
      - If not recording, return error
      - Build the .editsession JSON object with version, scene, duration_ms, events array
      - Write to filePath_ using std::ofstream with nlohmann::json pretty print (indent=2)
      - Set recording_ = false
      - Return `{"ok": true, "file": filePath_, "events": events_.size(), "duration_ms": duration}`
    - `nlohmann::json status()` — return recording state, duration, event count
    - `void recordCommand(const std::string& cmd, const nlohmann::json& args)` — if recording, push `{"t": elapsed_ms, "type": "command", "cmd": cmd, "args": args}` to events_
    - `void recordSnapshot(const nlohmann::json& stateData)` — if recording, push `{"t": elapsed_ms, "type": "snapshot", "data": stateData}`
    - Called from DebugHarness: after each command.* dispatch, call `recorder.recordCommand(cmd, args)` if recorder is recording

    **2. Create SessionPlayer** (`src/editor/debug/SessionPlayer.h/.cpp`):
    - Holds: loaded events vector, playback index, playback start time, paused flag, CommandRegistry reference (set via init)
    - `nlohmann::json load(const std::string& file)` — read .editsession file, parse JSON, store events, return ok + event count
    - `nlohmann::json play()` — set playing=true, record start time, reset index
    - `nlohmann::json pause()` — set paused=true
    - `nlohmann::json step()` — if events remain, dispatch next event's command through registry, advance index
    - `void tick(CommandRegistry& registry)` — called per frame from DebugHarness.poll():
      - If not playing or paused, return
      - Check elapsed time since play start
      - While next event's `t` <= elapsed_ms: dispatch command events through registry, advance index
      - For snapshot events: log expected vs actual state (don't enforce yet — future verification)
      - When all events consumed, set playing=false, log completion

    **3. Wire record/replay commands in DebugHarness::init()**:
    - `record.start` → `recorder.start(args["file"], doc.scenePath())`
    - `record.stop` → `recorder.stop()`
    - `record.status` → `recorder.status()`
    - `replay.load` → `player.load(args["file"])`
    - `replay.play` → `player.play()`
    - `replay.pause` → `player.pause()`
    - `replay.step` → `player.step()`
    - In poll(), after server.poll(), call `player.tick(registry)`
    - After dispatching any command.* through the registry, call `recorder.recordCommand(cmd, args)` if recording

    **4. Integrate into apps/level_editor/main.cpp**:
    - Add `#include "editor/debug/DebugHarness.h"` near top with other editor includes
    - After `editorViewportRenderer.init()` (around line 318), create the harness:
      ```cpp
      editor::DebugHarness debugHarness(document, selectedIds, ui, commandStack);
      debugHarness.init();
      ```
    - Inside the `renderFrame` lambda, after `content.pollMaterialHotReload(materialTextures)` (line ~427), add:
      ```cpp
      debugHarness.poll();
      ```
    - Before `imgui.shutdown()` (line ~2145), add:
      ```cpp
      debugHarness.shutdown();
      ```

    **5. Add debug-sessions to .gitignore**:
    - Append `assets/debug-sessions/` to the project .gitignore

    **Note on namespace**: All debug harness types should be in `namespace editor` (or no namespace, matching the rest of src/editor/ — check existing pattern). Looking at existing editor code, there is no namespace used, so keep these files without namespace as well.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -20</automated>
  </verify>
  <done>
    SessionRecorder captures timestamped command events and writes .editsession JSON files. SessionPlayer loads and replays .editsession files through the command registry. DebugHarness is instantiated in main.cpp, polls per frame, and shuts down cleanly. assets/debug-sessions/ is gitignored. The level-editor executable compiles and links with the complete debug harness.
  </done>
</task>

<task type="auto">
  <name>Task 3: Create editor-ctl.py CLI tool and verify end-to-end socket communication</name>
  <files>
    tools/editor-ctl.py
  </files>
  <action>
    **1. Create tools/editor-ctl.py**:
    - Shebang: `#!/usr/bin/env python3`
    - Pure stdlib — no third-party dependencies (uses `socket`, `json`, `glob`, `sys`, `argparse`)
    - Auto-discover socket: `glob.glob("/tmp/pixel-roguelike-editor-*.sock")`, take the first match. Error if none found.
    - `--socket PATH` flag to override auto-discovery
    - Usage: `python3 tools/editor-ctl.py <command> [args_json]`
    - Examples:
      ```
      python3 tools/editor-ctl.py inspect.selection
      python3 tools/editor-ctl.py command.set_gizmo '{"mode": "Scale"}'
      python3 tools/editor-ctl.py record.start
      python3 tools/editor-ctl.py record.stop
      ```
    - Implementation:
      1. Parse command and optional args JSON from argv
      2. Connect to Unix socket (AF_UNIX, SOCK_STREAM)
      3. Build request JSON: `{"id": 1, "cmd": command, "args": parsed_args_or_empty_dict}`
      4. Send as UTF-8 bytes + `\n`
      5. Receive response (read until `\n`)
      6. Parse response JSON, pretty-print to stdout
      7. Exit 0 if `ok=true`, exit 1 if `ok=false`
    - Add a `--raw` flag that prints the raw JSON without pretty-printing (useful for piping)
    - Add a `--wait SECONDS` timeout flag (default 5 seconds) for socket read timeout
    - Handle connection errors gracefully: "Could not connect to editor. Is it running?"

    **2. Verify end-to-end by building and checking the socket path output**:
    - Build level-editor: `cmake --build build --target level-editor`
    - The harness logs the socket path on init via spdlog — this confirms the socket layer initializes
    - Run editor-ctl.py with no editor running to verify it handles the "no socket found" case gracefully
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && python3 tools/editor-ctl.py inspect.selection 2>&1; echo "Exit code: $?"</automated>
  </verify>
  <done>
    editor-ctl.py exists at tools/editor-ctl.py, auto-discovers the Unix socket, sends JSON commands, and prints responses. When no editor is running, it prints a clear error message and exits with code 1. The complete debug harness system is ready: socket server + command registry + inspector + commander + recorder + player + CLI tool.
  </done>
</task>

</tasks>

<verification>
1. `cmake --build build --target level-editor` compiles without errors
2. `python3 tools/editor-ctl.py inspect.selection` (with no editor running) prints a graceful error
3. Launch the editor, then run `python3 tools/editor-ctl.py inspect.selection` — should return JSON with selection state
4. Run `python3 tools/editor-ctl.py command.set_gizmo '{"mode": "Scale"}'` — gizmo should change in editor
5. `python3 tools/editor-ctl.py inspect.panels` — returns panel visibility map
6. `python3 tools/editor-ctl.py record.start` then `record.stop` — writes .editsession file to assets/debug-sessions/
</verification>

<success_criteria>
- Editor debug harness compiles as part of the editor static library (not linked into game)
- Unix socket created at /tmp/pixel-roguelike-editor-{pid}.sock on editor startup
- All inspect.* commands return correct JSON state
- command.select_entity, deselect_all, set_gizmo, undo, redo, toggle_panel work
- Session recording writes valid .editsession JSON files
- editor-ctl.py CLI tool communicates with the running editor
- No impact on game executable build
</success_criteria>

<output>
After completion, create `.planning/quick/260402-okw-implement-editor-debug-harness-with-unix/260402-okw-SUMMARY.md`
</output>
