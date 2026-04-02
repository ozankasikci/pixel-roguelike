---
phase: quick
plan: 260402-prr
type: execute
wave: 1
depends_on: []
files_modified:
  - tests/editor/test_command_registry.cpp
  - tests/editor/test_session_record_replay.cpp
  - tests/editor/test_editor_debug_commands.cpp
  - tests/editor/CMakeLists.txt
autonomous: true
requirements: []
must_haves:
  truths:
    - "CommandRegistry dispatches registered commands and returns error for unknown commands"
    - "CommandRegistry catches handler exceptions and returns error JSON"
    - "SessionRecorder captures events with timestamps and writes valid JSON files"
    - "SessionPlayer loads session files and replays command events through CommandRegistry"
    - "EditorCommander select/deselect/setGizmo/togglePanel mutate editor state correctly"
    - "EditorInspector undoStack and panels return correct JSON from editor state"
  artifacts:
    - path: "tests/editor/test_command_registry.cpp"
      provides: "CommandRegistry unit tests"
    - path: "tests/editor/test_session_record_replay.cpp"
      provides: "SessionRecorder + SessionPlayer unit tests"
    - path: "tests/editor/test_editor_debug_commands.cpp"
      provides: "EditorCommander + EditorInspector unit tests"
  key_links:
    - from: "tests/editor/CMakeLists.txt"
      to: "pixel_roguelike_add_test()"
      via: "CMake test registration"
      pattern: "pixel_roguelike_add_test"
---

<objective>
Write unit tests for the editor debug harness components (src/editor/debug/).

Purpose: Verify the correctness of CommandRegistry dispatch, JSON protocol handling, SessionRecorder event capture, SessionPlayer replay, EditorCommander mutations, and EditorInspector queries -- all without requiring OpenGL or a running editor window.

Output: Three standalone test executables registered via pixel_roguelike_add_test(), following the project's assert-based test pattern.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@tests/editor/CMakeLists.txt
@tests/editor/test_editor_command_stack.cpp
@tests/common/TestSupport.h
@tests/cmake/TestSupport.cmake
@src/editor/debug/CommandRegistry.h
@src/editor/debug/CommandRegistry.cpp
@src/editor/debug/SessionRecorder.h
@src/editor/debug/SessionRecorder.cpp
@src/editor/debug/SessionPlayer.h
@src/editor/debug/SessionPlayer.cpp
@src/editor/debug/EditorCommander.h
@src/editor/debug/EditorCommander.cpp
@src/editor/debug/EditorInspector.h
@src/editor/debug/EditorInspector.cpp
@src/editor/scene/EditorSceneDocument.h
@src/editor/ui/LevelEditorUi.h
@src/editor/core/EditorCommand.h
</context>

<tasks>

<task type="auto">
  <name>Task 1: CommandRegistry and Session Record/Replay tests</name>
  <files>tests/editor/test_command_registry.cpp, tests/editor/test_session_record_replay.cpp</files>
  <action>
Create two test files:

**test_command_registry.cpp** -- Tests for CommandRegistry in isolation:
1. Register a command that returns `{"value": 42}`, dispatch it, verify response has `ok=true` and `data.value=42`.
2. Dispatch an unregistered command name, verify response has `ok=false` and `error` contains "Unknown command".
3. Register a command that returns JSON already containing `"ok"` key -- verify the response passes through unchanged (no double-wrapping).
4. Register a command handler that throws `std::runtime_error("boom")`, dispatch it, verify response has `ok=false` and `error` contains "boom".
5. Register two commands with different names, verify each dispatches to the correct handler (not cross-wired).
6. Re-register a command name with a new handler, verify the new handler is used.

**test_session_record_replay.cpp** -- Tests for SessionRecorder + SessionPlayer:
1. **Recorder start/stop lifecycle**: Create a SessionRecorder, call `start()` with a temp file path and a scene path. Verify `isRecording()` is true and response has `ok=true`. Call `stop()`, verify `isRecording()` is false, response has `ok=true`, and the file exists on disk.
2. **Recorder writes valid JSON**: After stop(), read the output file, parse as JSON, verify it contains `version`, `scene`, `duration_ms`, and `events` array. Verify the first event is type "snapshot" with t=0.
3. **Recorder captures commands**: While recording, call `recordCommand("test.cmd", {{"x", 1}})` twice. Stop. Read file, verify events array has 3 entries (1 initial snapshot + 2 commands). Verify command events have `type="command"`, `cmd`, `args`, and `t` >= 0.
4. **Recorder captures snapshots**: While recording, call `recordSnapshot({{"selection", "box"}})`. Stop. Verify the snapshot event appears in events array with `type="snapshot"`.
5. **Recorder rejects double-start**: Start recording, call `start()` again, verify response has `ok=false` with "Already recording".
6. **Recorder rejects stop-when-not-recording**: Call `stop()` without starting, verify response has `ok=false`.
7. **Recorder ignores events when not recording**: Call `recordCommand()` before starting -- verify no crash and after start+stop the events array has only the initial snapshot.
8. **Player load**: Write a minimal session JSON file to a temp path (`{"version":1,"scene":"test.scene","duration_ms":100,"events":[{"t":0,"type":"snapshot","data":{"scene":"test.scene"}},{"t":50,"type":"command","cmd":"echo","args":{}}]}`). Call `player.load(file)`, verify response `ok=true` and `events=2`.
9. **Player step**: After load, register an "echo" command in a CommandRegistry that sets a bool flag. Call `player.step(registry)` twice. Verify the flag was set on the second step (first event is a snapshot, second is the command).
10. **Player load missing file**: Call `player.load("nonexistent.file")`, verify response `ok=false`.
11. **Player play without load**: Call `player.play()` on a fresh player, verify `ok=false`.

Use `test_support::resetTempDirectory("debug_harness_tests")` for temp files. Clean up after each test. Include `common/TestSupport.h`. Both files use `#include <cassert>` and return 0 on success.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_command_registry test_session_record_replay && cd build && ctest -R "command_registry|session_record_replay" --output-on-failure</automated>
  </verify>
  <done>Both tests compile, link, and pass. CommandRegistry dispatch/error/exception paths all verified. SessionRecorder writes valid JSON with correct event structure. SessionPlayer loads and steps through events.</done>
</task>

<task type="auto">
  <name>Task 2: EditorCommander and EditorInspector tests</name>
  <files>tests/editor/test_editor_debug_commands.cpp</files>
  <action>
Create a single test file that tests EditorCommander mutations and the subset of EditorInspector that does NOT require ImGui context (undoStack, panels).

**Test setup** (reuse the pattern from test_editor_command_stack.cpp):
- Create an `EditorSceneDocument`, call `clear()`, add a mesh object via `addMesh(makeMeshPlacement())`.
- Create `std::vector<uint64_t> selectedIds`.
- Create `EditorUiState ui` with defaults.
- Create `EditorCommandStack cmdStack`, call `reset(document)`.
- Construct `EditorCommander commander(doc, selectedIds, ui, cmdStack)`.

**EditorCommander tests:**
1. **selectEntity**: Call `commander.selectEntity({{"name", label}})` where `label` is the mesh label from `editorSceneObjectLabel()`. Verify `ok=true` and `selectedIds` has 1 entry matching the object id.
2. **selectEntity not found**: Call with `name="nonexistent"`, verify `ok=false` and error contains "not found".
3. **selectEntity missing name**: Call with empty args `{}`, verify `ok=false`.
4. **deselectAll**: Set `selectedIds = {someId}`, call `deselectAll({})`, verify `ok=true` and `selectedIds` is empty.
5. **setGizmo Translate/Rotate/Scale**: For each mode string, call `setGizmo({{"mode", mode}})`, verify `ok=true` and `ui.tool` matches the expected `EditorTransformTool` enum.
6. **setGizmo invalid**: Call with `mode="Invalid"`, verify `ok=false`.
7. **togglePanel**: Set `ui.showOutliner = true`, call `togglePanel({{"panel", "outliner"}})`, verify `ui.showOutliner` is now false. Call again, verify it's true. Repeat for "inspector", "asset_browser", "environment", "viewport", "build_output".
8. **togglePanel unknown**: Call with `panel="unknown_panel"`, verify `ok=false`.
9. **undo with nothing to undo**: Call `commander.undo({})`, verify `ok=false`.
10. **undo/redo roundtrip**: Push a document state change to cmdStack (move mesh position), then call `commander.undo({})` verify `ok=true`, call `commander.redo({})` verify `ok=true`.

**EditorInspector tests** (subset that works without ImGui context):
- Construct `EditorInspector inspector(doc, selectedIds, ui, cmdStack)`.
11. **undoStack**: With empty cmdStack, call `inspector.undoStack()`, verify `ok=true`, `data.can_undo=false`, `data.can_redo=false`. Push a command, verify `can_undo=true`.
12. **panels**: Call `inspector.panels()`, verify `ok=true` and data matches `ui` field values (`showOutliner`, `showInspector`, etc.).

NOTE: Do NOT test `inspector.selection()`, `inspector.gizmoState()`, `inspector.inputState()`, or `inspector.frameStats()` -- they call `editorGizmoIsHot()` (ImGuizmo) or `ImGui::GetMousePos()` which require an initialized ImGui context. Those require integration tests with a running editor.

Helper: Create a local `makeMeshPlacement()` identical to the one in test_editor_command_stack.cpp. Use `editorSceneObjectLabel()` from EditorSceneDocument.h to get the label for selectEntity test.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_editor_debug_commands && cd build && ctest -R "editor_debug_commands" --output-on-failure</automated>
  </verify>
  <done>test_editor_debug_commands compiles, links, and passes. EditorCommander select/deselect/gizmo/panel/undo/redo paths verified. EditorInspector undoStack and panels return correct JSON.</done>
</task>

<task type="auto">
  <name>Task 3: Register all three tests in CMake</name>
  <files>tests/editor/CMakeLists.txt</files>
  <action>
Append three `pixel_roguelike_add_test()` calls to `tests/editor/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_command_registry
    SOURCES test_command_registry.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_session_record_replay
    SOURCES test_session_record_replay.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_debug_commands
    SOURCES test_editor_debug_commands.cpp
    LIBRARIES editor
    LABELS editor
)
```

All three link against `editor` which transitively provides `nlohmann_json`, `gameplay`, and all other needed deps.

After adding, run cmake configure to verify no errors, then build all three targets, then run all three tests.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_command_registry test_session_record_replay test_editor_debug_commands && cd build && ctest -R "command_registry|session_record_replay|editor_debug_commands" --output-on-failure</automated>
  </verify>
  <done>All three test executables compile, link, and pass. `ctest -L editor` includes the new tests alongside existing editor tests.</done>
</task>

</tasks>

<verification>
Run the full editor test label to confirm nothing is broken:
```bash
cd build && ctest -L editor --output-on-failure
```
All existing and new editor tests pass.
</verification>

<success_criteria>
- Three new test executables: test_command_registry, test_session_record_replay, test_editor_debug_commands
- All three pass via `ctest -L editor`
- CommandRegistry: dispatch, unknown command, exception handling, re-registration tested
- SessionRecorder: start/stop lifecycle, JSON output, command/snapshot capture, error cases tested
- SessionPlayer: load, step, play-without-load, missing file tested
- EditorCommander: selectEntity, deselectAll, setGizmo, togglePanel, undo/redo tested
- EditorInspector: undoStack, panels queries tested
- No ImGui/OpenGL/window initialization required in any test
</success_criteria>

<output>
After completion, create `.planning/quick/260402-prr-write-tests-for-the-editor-debug-harness/260402-prr-SUMMARY.md`
</output>
