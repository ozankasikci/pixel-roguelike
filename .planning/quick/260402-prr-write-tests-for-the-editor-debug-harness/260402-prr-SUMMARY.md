---
phase: quick
plan: 260402-prr
subsystem: editor/debug
tags: [testing, editor, debug-harness, command-registry, session-recorder, session-player]
key-files:
  created:
    - tests/editor/test_command_registry.cpp
    - tests/editor/test_session_record_replay.cpp
    - tests/editor/test_editor_debug_commands.cpp
  modified:
    - tests/editor/CMakeLists.txt
decisions:
  - "EditorInspector undoStack() and panels() are the only safe methods to test without ImGui context; selection(), gizmoState(), inputState(), and frameStats() call ImGuizmo/ImGui APIs"
  - "CMakeLists entries added to tests/editor/CMakeLists.txt; all three link against editor which transitively provides nlohmann_json and all other needed deps"
metrics:
  duration: "~15 min"
  completed: "2026-04-02"
  tasks: 3
  files: 4
---

# Phase quick Plan 260402-prr: Write Tests for the Editor Debug Harness Summary

Three standalone test executables covering the editor debug harness components — CommandRegistry dispatch and error handling, SessionRecorder write and SessionPlayer step replay, EditorCommander state mutations, and EditorInspector undoStack/panels queries — all passing without OpenGL or ImGui context.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | CommandRegistry and Session Record/Replay tests | b9a826f | tests/editor/test_command_registry.cpp, tests/editor/test_session_record_replay.cpp |
| 2 | EditorCommander and EditorInspector tests | b5f83e7 | tests/editor/test_editor_debug_commands.cpp |
| 3 | Register all three tests in CMake | 28e297f | tests/editor/CMakeLists.txt |

## What Was Built

### test_command_registry (6 tests)
- Dispatch to registered handler returns `ok=true` and `data.value=42`
- Unknown command returns `ok=false` with "Unknown command" in error
- Handler returning JSON with existing `ok` key passes through unchanged (no double-wrapping)
- Throwing handler returns `ok=false` with exception message in error
- Two commands dispatch to correct handlers independently
- Re-registering a command name uses the new handler

### test_session_record_replay (11 tests)
- Start/stop lifecycle: `isRecording()` state transitions, file exists after stop
- Valid JSON output: `version`, `scene`, `duration_ms`, `events` array, first event is snapshot at `t=0`
- Command capture: 1 initial snapshot + 2 commands yields 3 events with correct structure
- Snapshot capture: explicit `recordSnapshot()` appears in events with `type="snapshot"`
- Double-start rejected with "Already recording"
- Stop-when-not-recording rejected with `ok=false`
- Events recorded before `start()` are silently ignored
- Player load: writes minimal session JSON, `player.load()` returns `ok=true` and `events=2`
- Player step: snapshot event does not call handler; command event dispatches to registry handler
- Player load missing file returns `ok=false`
- Player `play()` without load returns `ok=false`

### test_editor_debug_commands (19 tests)
- `selectEntity` finds object by `editorSceneObjectLabel()` result, sets `selectedIds`
- `selectEntity` with unknown name returns `ok=false` with "not found"
- `selectEntity` with missing `name` arg returns `ok=false`
- `deselectAll` clears `selectedIds`
- `setGizmo` Translate/Rotate/Scale sets `ui.tool` to correct enum value
- `setGizmo` with invalid mode returns `ok=false`
- `togglePanel` toggles all 6 panel flags (outliner, inspector, asset_browser, environment, viewport, build_output)
- `togglePanel` with unknown panel name returns `ok=false`
- `undo` with empty stack returns `ok=false`
- `undo`/`redo` roundtrip reverts and re-applies mesh position change
- `EditorInspector::undoStack()` returns `can_undo=false`/`can_redo=false` on empty stack; `can_undo=true` after a push
- `EditorInspector::panels()` returns data matching all six `EditorUiState` panel fields

## Deviations from Plan

None — plan executed exactly as written.

## Test Results

```
9/9 tests passed (100%) — ctest -L editor
  editor_selection       PASSED
  editor_command_stack   PASSED
  editor_layout_presets  PASSED
  editor_hierarchy       PASSED
  editor_asset_browser   PASSED
  editor_runtime_preview PASSED
  command_registry       PASSED
  session_record_replay  PASSED
  editor_debug_commands  PASSED
```

## Known Stubs

None.

## Self-Check: PASSED

- tests/editor/test_command_registry.cpp: FOUND
- tests/editor/test_session_record_replay.cpp: FOUND
- tests/editor/test_editor_debug_commands.cpp: FOUND
- tests/editor/CMakeLists.txt: MODIFIED (contains all three pixel_roguelike_add_test entries)
- Commits b9a826f, b5f83e7, 28e297f: FOUND in git log
- All 9 ctest -L editor tests: PASSED
