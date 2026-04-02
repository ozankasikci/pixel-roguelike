---
phase: quick
plan: 260402-tpl
type: execute
wave: 1
depends_on: []
files_modified:
  - /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/editor_debug_harness.md
  - /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/MEMORY.md
autonomous: true
requirements: [document-debug-harness]
must_haves:
  truths:
    - "Future Claude conversations automatically know the debug harness exists and how to connect"
    - "Future conversations can look up any command's parameters and return values without reading source"
    - "Future conversations know common debugging workflows for editor issues"
  artifacts:
    - path: "/Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/editor_debug_harness.md"
      provides: "Complete debug harness reference documentation"
    - path: "/Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/MEMORY.md"
      provides: "Updated index pointing to harness doc"
---

<objective>
Create persistent memory documentation for the EditorCommander debugging harness so future Claude conversations automatically know how to use it when debugging editor issues.

Purpose: The harness is a powerful tool (~1,600 lines, 33 commands) but future conversations won't know it exists unless it's in memory. This bridges that gap.
Output: A comprehensive memory file with connection instructions, full command reference, and debugging workflows.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
Source files (already analyzed — executor should NOT re-read these):
- src/editor/debug/DebugHarness.h/cpp — facade, command registration
- src/editor/debug/EditorCommander.h/cpp — 13 mutating commands
- src/editor/debug/EditorInspector.h/cpp — 13 read-only queries
- src/editor/debug/CommandRegistry.h/cpp — string-to-handler dispatcher
- src/editor/debug/DebugServer.h/cpp — Unix socket server
- src/editor/debug/SessionRecorder.h/cpp — event capture
- src/editor/debug/SessionPlayer.h/cpp — replay

Existing memory format reference:
- /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/feedback_harness_debugging.md
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create comprehensive debug harness memory file</name>
  <files>/Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/editor_debug_harness.md</files>
  <action>
Create a memory file with frontmatter `name: Editor Debug Harness Reference`, `description: Complete reference for the Unix socket debug harness that allows programmatic control and inspection of the level editor`, `type: reference`.

The content MUST include all of the following sections. This is the complete command reference extracted from source — the executor should write this directly without re-reading source files.

**Section 1: Overview**
- The harness lives in `src/editor/debug/` (~1,600 lines, 8 files)
- Architecture: DebugHarness (facade) -> EditorCommander (mutating), EditorInspector (read-only), CommandRegistry (dispatcher), DebugServer (Unix socket), SessionRecorder/SessionPlayer (replay)
- Automatically starts when the level-editor launches

**Section 2: Connection**
- Socket path: `/tmp/pixel-roguelike-editor-{pid}.sock` (Unix domain socket)
- Find socket: `ls /tmp/pixel-roguelike-editor-*.sock`
- Protocol: Line-delimited JSON (one JSON object per line, newline terminated)
- Request format: `{"id": N, "cmd": "namespace.command", "args": {...}}`
- Response format: `{"id": N, "ok": true, "data": {...}}` or `{"id": N, "ok": false, "error": "..."}`
- Connect with: `nc -U /tmp/pixel-roguelike-editor-{pid}.sock` or programmatically via Python/socat

**Section 3: Full Command Reference**
Each command must list: command string, parameters (with types and defaults), return data fields.

Inspect commands (read-only, no args unless noted):
- `inspect.selection` — returns: `{selected: string[], gizmo: "Translate"|"Rotate"|"Scale", gizmo_active: bool}`
- `inspect.gizmo_state` — returns: `{mode: string, active: bool}`
- `inspect.input_state` — returns: `{mouse_x, mouse_y, mouse_button_0/1/2: bool, key_ctrl/shift/alt: bool}`
- `inspect.frame_stats` — returns: `{fps: float, delta_time: float}`
- `inspect.undo_stack` — returns: `{can_undo: bool, can_redo: bool, undo_label: string, redo_label: string}`
- `inspect.panels` — returns: `{outliner, inspector, asset_browser, environment, viewport, build_output: bool}`
- `inspect.imgui_capture` — returns: `{want_capture_mouse, want_capture_keyboard, want_text_input: bool, hovered_window: string, active_id: int, active_id_window: string}`
- `inspect.imguizmo_state` — returns: `{is_over, is_using, is_using_any, is_using_view_manipulate, is_view_manipulate_hovered: bool}`
- `inspect.gizmo_detailed` — returns: combined gizmo + mouse + selection + undo state (most useful for debugging gizmo issues)
- `inspect.entities` — returns: array of `{id: uint64, label: string, kind: string, world_position: {x,y,z}}`
- `inspect.world_to_screen` — args: `{x, y, z: float}` — returns: `{x, y: float, visible: bool}` (viewport pixel coords)
- `inspect.camera` — returns: `{position: {x,y,z}, yaw, pitch, fov, orbit_distance: float, orbit_pivot: {x,y,z}, orbit_pivot_valid: bool}`
- `inspect.gizmo_screen_pos` — returns: `{x, y: float}` (ImGuizmo screen center in GLFW cursor space; only valid after Manipulate() called)

Command commands (mutating):
- `command.select_entity` — args: `{name: string}` (required) — selects entity by label
- `command.deselect_all` — no args — clears selection
- `command.set_gizmo` — args: `{mode: "Translate"|"Rotate"|"Scale"}` (required)
- `command.undo` — no args — undoes last command (errors if nothing to undo)
- `command.redo` — no args — redoes last undone command
- `command.toggle_panel` — args: `{panel: "outliner"|"inspector"|"asset_browser"|"environment"|"viewport"|"build_output"}` (required)
- `command.key_press` — args: `{key: string}` (required, e.g. "W", "Delete", "F1"), optional: `{ctrl, super, shift, alt: bool}`
- `command.mouse_click` — args: `{x, y: float}` (required), optional: `{button: int}` (default 0, range 0-4)
- `command.drag` — args: `{start_x, start_y, end_x, end_y: float}` (required), optional: `{button: int, steps: int (default 10), hold: bool (default false)}` — if hold=true, does not release button
- `command.mouse_release` — args: optional `{button: int}` (default 0) — releases held mouse button
- `command.focus_entity` — args: `{name: string}` (required) — animates camera to focus on entity (takes ~0.3s / 18 frames)
- `command.gizmo_drag` — args: optional `{direction: "right"|"left"|"up"|"down" or {dx, dy}, distance: float (default 50), steps: int (default 10)}` — drags from gizmo center in given direction. Requires entity selected and camera focused. Returns gizmo_center and drag_end coords.
- `command.wait_events` — no args — returns: `{pending_events: int, camera_animating: bool, idle: bool}` — poll this to wait for events to be processed

Record/Replay commands:
- `record.start` — args: `{file: string}` — starts recording commands to file
- `record.stop` — no args — stops recording, saves file
- `record.status` — no args — returns recording state
- `replay.load` — args: `{file: string}` — loads recorded session
- `replay.play` — no args — starts playback
- `replay.pause` — no args — pauses playback
- `replay.step` — no args — executes next recorded command

**Section 4: Common Debugging Workflows**

Workflow 1 — Investigate a gizmo/transform bug:
1. Connect to socket
2. `inspect.entities` to list all objects and find the target name
3. `command.select_entity` with the target name
4. `command.focus_entity` to aim the camera at it
5. Poll `command.wait_events` until `idle: true`
6. `inspect.gizmo_screen_pos` to verify gizmo is visible on screen
7. `command.set_gizmo` to set the desired transform mode
8. `inspect.gizmo_detailed` to capture pre-drag state
9. `command.gizmo_drag` to perform the drag
10. Poll `command.wait_events` then `inspect.gizmo_detailed` to capture post-drag state
11. Compare pre/post state for anomalies

Workflow 2 — Investigate selection/picking bugs:
1. `inspect.entities` to see all entities with world positions
2. `inspect.world_to_screen` to convert an entity's world pos to screen coords
3. `command.mouse_click` at those screen coordinates
4. `inspect.selection` to check if the entity was picked
5. `inspect.imgui_capture` to check if ImGui is capturing mouse (interfering)

Workflow 3 — Test undo/redo:
1. `inspect.undo_stack` to check initial state
2. Perform operations (select, transform, etc.)
3. `command.undo` and verify state reverted
4. `command.redo` and verify state restored
5. `inspect.undo_stack` to verify labels

Workflow 4 — Record and replay a bug reproduction:
1. `record.start` with a filename
2. Perform the steps that trigger the bug
3. `record.stop` to save
4. `replay.load` then `replay.play` to reproduce deterministically

**Section 5: Shell One-Liner Examples**
- Connect: `nc -U /tmp/pixel-roguelike-editor-*.sock`
- List entities: `echo '{"id":1,"cmd":"inspect.entities","args":{}}' | nc -U /tmp/pixel-roguelike-editor-*.sock`
- Select entity: `echo '{"id":1,"cmd":"command.select_entity","args":{"name":"wall_01"}}' | nc -U /tmp/pixel-roguelike-editor-*.sock`

**Section 6: Key Design Notes**
- Events are queued into ImGui's input event queue and processed across subsequent frames — not instant
- `command.gizmo_drag` auto-locates the gizmo screen center via ImGuizmo::GetScreenCenter() — you don't need to compute coordinates
- Always `command.focus_entity` and wait for `idle: true` before `command.gizmo_drag` — the gizmo must be rendered on screen
- The harness runs on the main thread via `poll()` each frame — no threading issues
- Socket path includes PID, so multiple editor instances don't conflict
  </action>
  <verify>
    <automated>test -f /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/editor_debug_harness.md && head -5 /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/editor_debug_harness.md | grep -q "name:" && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>Memory file exists with frontmatter, all 33 commands documented with params/returns, 4 debugging workflows, shell examples, and design notes.</done>
</task>

<task type="auto">
  <name>Task 2: Update MEMORY.md index</name>
  <files>/Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/MEMORY.md</files>
  <action>
Read the existing MEMORY.md at `/Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/MEMORY.md` and append a new line entry pointing to the harness doc:

```
- [Editor Debug Harness Reference](editor_debug_harness.md) — Complete reference for the Unix socket debug harness: 33 commands for programmatic editor control, inspection, and session replay
```

Preserve all existing entries. The new entry should be added as the last line.
  </action>
  <verify>
    <automated>grep -q "editor_debug_harness.md" /Users/ozan/.claude/projects/-Users-ozan-Projects-gsd-3d-roguelike/memory/MEMORY.md && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>MEMORY.md index contains a pointer to editor_debug_harness.md with a descriptive summary.</done>
</task>

</tasks>

<verification>
- Memory file exists at the correct path with valid frontmatter
- MEMORY.md index updated with pointer to new file
- All 33 commands (13 inspect + 13 command + 4 record + 3 replay) are documented
- Debugging workflows are actionable step-by-step sequences
</verification>

<success_criteria>
Future Claude conversations loading memory for this project will see the harness reference and can use it to programmatically debug editor issues without reading source code.
</success_criteria>

<output>
After completion, create `.planning/quick/260402-tpl-document-the-editorcommander-debugging-h/260402-tpl-SUMMARY.md`
</output>
