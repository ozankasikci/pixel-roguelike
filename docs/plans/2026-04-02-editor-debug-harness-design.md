# Editor Debug Harness Design

Remote control and inspection system for the level editor, enabling programmatic debugging, automated testing, and session recording via Unix domain sockets.

## Goals

- Allow external tools (Claude, scripts) to inspect and control the running editor
- Record editor sessions for deterministic replay and regression testing
- Zero impact on release builds — lives entirely in the editor library

## Architecture

### Components

```
src/editor/debug/
  DebugHarness.h/cpp        Top-level facade: init, shutdown, poll
  DebugServer.h/cpp          Unix socket listener, JSON parse/dispatch
  EditorInspector.h/cpp      Read-only state queries
  EditorCommander.h/cpp      Mutating editor commands
  SessionRecorder.h/cpp      Event capture & serialization
  SessionPlayer.h/cpp        Replay from .editsession files
  CommandRegistry.h/cpp      Maps command strings to handler functions

tools/
  editor-ctl.py              CLI wrapper for socket communication
```

### Integration

- All files live in `src/editor/debug/`, compiled as part of the `editor` static library
- The `editor` library is only linked by the `level-editor` executable — never the game
- No preprocessor guards needed; architectural separation handles it

### Main Loop

```cpp
// After editor initialization
editor::DebugHarness debugHarness(sceneDoc, editorUi, window);
debugHarness.init();

// Inside per-frame render lambda
debugHarness.poll();  // Non-blocking socket check + command dispatch

// At shutdown
debugHarness.shutdown();
```

## Communication Protocol

Unix domain socket at `/tmp/pixel-roguelike-editor-{pid}.sock`.
Newline-delimited JSON, request/response pattern.

### Request

```json
{"id": 1, "cmd": "inspect.selection", "args": {}}
{"id": 2, "cmd": "command.select_entity", "args": {"name": "arch_gothic_01"}}
{"id": 3, "cmd": "command.drag", "args": {"from": [400, 300], "to": [400, 200], "button": 0}}
```

### Response

```json
{"id": 1, "ok": true, "data": {"selected": ["arch_gothic_01"], "gizmo": "Scale", "gizmo_active": true}}
{"id": 2, "ok": true}
{"id": 3, "ok": true}
```

### Error

```json
{"id": 5, "ok": false, "error": "Entity not found: foo_bar"}
```

## Command Reference

### inspect.* (read-only)

| Command | Args | Returns |
|---------|------|---------|
| `inspect.selection` | — | Selected entity names, gizmo mode, gizmo_active flag |
| `inspect.gizmo_state` | — | Current gizmo mode, whether actively dragging, hover state |
| `inspect.input_state` | — | Mouse position, button states, modifier keys, cursor lock |
| `inspect.frame_stats` | — | FPS, frame time, triangle count, draw calls |
| `inspect.component` | `entity`, `component` | Component data for named entity |
| `inspect.undo_stack` | — | Undo/redo stack depth and top command descriptions |
| `inspect.panels` | — | Visible panels and their docked positions |

### command.* (mutating)

| Command | Args | Effect |
|---------|------|--------|
| `command.select_entity` | `name` | Select entity by node name |
| `command.deselect_all` | — | Clear selection |
| `command.set_gizmo` | `mode` (Translate/Rotate/Scale) | Switch gizmo mode |
| `command.undo` | — | Trigger undo |
| `command.redo` | — | Trigger redo |
| `command.key_press` | `key`, `mods` | Simulate key press |
| `command.mouse_click` | `x`, `y`, `button` | Simulate click at viewport coords |
| `command.drag` | `from`, `to`, `button`, `steps` | Simulate drag gesture over N steps |
| `command.toggle_panel` | `panel` | Toggle panel visibility |

### record.* / replay.*

| Command | Args | Effect |
|---------|------|--------|
| `record.start` | `file` (optional) | Begin recording session |
| `record.stop` | — | Stop and save recording |
| `record.status` | — | Recording active, duration, event count |
| `replay.load` | `file` | Load a .editsession file |
| `replay.play` | — | Begin playback at recorded speed |
| `replay.pause` | — | Pause playback |
| `replay.step` | — | Advance one event |

## Recording Format

`.editsession` JSON files stored in `assets/debug-sessions/` (gitignored).

```json
{
  "version": 1,
  "scene": "initial_scene.scene",
  "duration_ms": 12500,
  "events": [
    {"t": 0, "type": "snapshot", "data": {"selected": [], "gizmo": "Translate"}},
    {"t": 200, "type": "command", "cmd": "select_entity", "args": {"name": "wall_01"}},
    {"t": 500, "type": "snapshot", "data": {"selected": ["wall_01"], "gizmo": "Translate"}},
    {"t": 800, "type": "command", "cmd": "set_gizmo", "args": {"mode": "Scale"}},
    {"t": 1000, "type": "command", "cmd": "drag", "args": {"from": [400, 300], "to": [400, 150]}}
  ]
}
```

Snapshots serve as assertions during replay — divergences between actual and recorded state are reported as test failures.

## Dependencies

- **JSON**: nlohmann/json (header-only, already widely used in C++ ecosystem) or minimal hand-rolled parser
- **Socket**: POSIX `sys/socket.h`, `sys/un.h` — standard on macOS/Linux
- **No new external libraries** beyond JSON parsing

## CLI Helper

`tools/editor-ctl.py` provides command-line access:

```bash
python3 tools/editor-ctl.py inspect.selection
python3 tools/editor-ctl.py command.set_gizmo '{"mode": "Scale"}'
python3 tools/editor-ctl.py command.drag '{"from": [400, 300], "to": [400, 200]}'
python3 tools/editor-ctl.py record.start
python3 tools/editor-ctl.py record.stop
```

Auto-discovers the socket via `/tmp/pixel-roguelike-editor-*.sock` glob.
