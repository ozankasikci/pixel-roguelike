# Quick Task: Fix Editor High CPU Usage - Research

**Researched:** 2026-04-01
**Domain:** GLFW main loop throttling, ImGui idle rendering
**Confidence:** HIGH

## Summary

The level editor's main loop (`apps/level_editor/main.cpp:2075-2078`) runs `window.pollEvents()` + `renderFrame()` in a tight while-loop. Even though vsync is enabled (`glfwSwapInterval(1)` at line 275), macOS vsync via OpenGL is unreliable (documented GLFW issues across Mojave, Catalina, Monterey, M1). The result is near-100% CPU usage even when the editor is idle.

The industry-standard fix for ImGui+GLFW editor applications is `glfwWaitEventsTimeout()` with a short timeout, gated by an "is animating" flag. This is the approach recommended by the ImGui wiki ("Implementing Power Save, aka Idling outside of ImGui") and used by production editors like lite-xl. The editor already has the infrastructure to detect when continuous rendering is needed (`cameraAnim.active`, `ui.playPreview`).

**Primary recommendation:** Replace `glfwPollEvents()` with `glfwWaitEventsTimeout(1.0 / idleFps)` when idle, keeping `glfwPollEvents()` when animations or runtime preview are active.

## Project Constraints (from CLAUDE.md)

- Engine: Custom C++ with OpenGL 4.1 Core Profile
- GLFW 3.4 for windowing
- Dear ImGui for editor UI
- Editor executable: `level-editor` (`apps/level_editor/main.cpp`)
- No commit co-authorship unless requested

## Current Editor Loop Analysis

```
// apps/level_editor/main.cpp:2075-2078
while (!window.shouldClose()) {
    window.pollEvents();    // <-- returns immediately, busy-spins
    renderFrame();          // <-- full ImGui + OpenGL render every iteration
}
```

Key observations:
- vsync is set: `glfwSwapInterval(1)` at line 275
- `renderFrame` is a lambda (line 411) doing full ImGui frame + OpenGL render
- `windowRefreshCallback` calls `renderFrame` during macOS live resize (lines 83-88)
- deltaTime is computed from `ImGui::GetIO().Framerate` (line 432), not from `Time` class
- Camera animation exists: `cameraAnim.active` flag (line 322, ticked at 1366)
- Runtime preview mode: `ui.playPreview && runtimePreviewSession.captured()` needs full-speed rendering

## GLFW Event Functions

| Function | Behavior | CPU When Idle | Use Case |
|----------|----------|---------------|----------|
| `glfwPollEvents()` | Returns immediately | HIGH (busy-loop) | Games, continuous rendering |
| `glfwWaitEvents()` | Blocks until event arrives | NEAR-ZERO | Fully event-driven apps |
| `glfwWaitEventsTimeout(seconds)` | Blocks until event OR timeout | LOW (wakes at timeout rate) | Editors with periodic updates |
| `glfwPostEmptyEvent()` | Wakes sleeping thread | N/A | Cross-thread wake signal |

Source: [GLFW Input Guide](https://www.glfw.org/docs/3.4/input_guide.html)

## Recommended Architecture

### Approach: Hybrid Poll/Wait with Activity Detection

```cpp
// Constants
constexpr double kIdleTimeoutSeconds = 1.0 / 15.0;  // ~15 FPS when idle
constexpr double kUnfocusedTimeoutSeconds = 1.0 / 5.0;  // ~5 FPS when unfocused

// In the main loop, BEFORE renderFrame():
bool needsContinuousRendering = cameraAnim.active
    || (ui.playPreview && runtimePreviewSession.captured())
    || ImGui::GetIO().WantCaptureMouse
    || ImGui::GetIO().WantCaptureKeyboard;

if (needsContinuousRendering) {
    glfwPollEvents();
} else if (glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == 0) {
    glfwWaitEventsTimeout(kUnfocusedTimeoutSeconds);
} else {
    glfwWaitEventsTimeout(kIdleTimeoutSeconds);
}
```

### Why This Pattern

1. **ImGui wiki recommendation** -- ocornut's official wiki page "Implementing Power Save, aka Idling outside of ImGui" prescribes exactly this: use backend `WaitForEventTimeout()` with a timeout of `1.0 / targetIdleFps`, gated by activity detection.

2. **Production-proven** -- lite-xl editor uses this exact pattern (focused=timeout, unfocused=longer timeout or indefinite wait).

3. **Godot's approach** -- Godot's "Low Processor Mode" inserts a sleep between frames (configurable microseconds). Same principle, different mechanism.

4. **Blender** -- Uses a 5ms sleep in idle loop on macOS; extends to 50ms for deeper idle. Event-driven redraw with platform-specific event checking.

### Why NOT Pure glfwWaitEvents()

- ImGui cursor blink animation would stop
- Material hot-reload polling (line 423: `content.pollMaterialHotReload`) needs periodic wakeup
- Any future background task (build system, etc.) needs periodic UI refresh
- `glfwWaitEventsTimeout` gives the same CPU savings with guaranteed periodic refresh

### Why NOT Just Rely on VSync

macOS vsync via OpenGL is unreliable:
- GLFW issue [#1337](https://github.com/glfw/glfw/issues/1337): Broken on Mojave
- GLFW issue [#1990](https://github.com/glfw/glfw/issues/1990): Broken on Monterey
- GLFW issue [#1834](https://github.com/glfw/glfw/issues/1834): Broken on M1 Macs (reports ~100fps regardless of swap interval)
- Even when working, vsync only throttles to display refresh rate (60-120Hz), which is still too much CPU for a static editor

## Implementation Details

### Where to Change

Only one file needs modification: `apps/level_editor/main.cpp`

Changes required:
1. **Add `needsContinuousRendering` computation** before the poll/wait call (lines ~2075-2078)
2. **Replace `window.pollEvents()` with conditional poll/wait** in the main loop
3. **Optionally add `waitEventsTimeout(double)` to Window class** for API consistency (or call `glfwWaitEventsTimeout` directly since `main.cpp` already includes `<GLFW/glfw3.h>`)

### Activity Signals to Check

| Signal | Variable | Meaning |
|--------|----------|---------|
| Camera animating | `cameraAnim.active` | Focus animation in progress |
| Runtime preview | `ui.playPreview && runtimePreviewSession.captured()` | Game running in editor |
| Mouse interaction | `ImGui::GetIO().WantCaptureMouse` | User interacting with ImGui |
| Keyboard interaction | `ImGui::GetIO().WantCaptureKeyboard` | User typing |
| Window unfocused | `glfwGetWindowAttrib(handle, GLFW_FOCUSED) == 0` | Editor in background |

**Important nuance:** `WantCaptureMouse` is true when hovering over ImGui windows, which covers most editor interaction. It will be false when mouse is over empty viewport space -- that is correct, viewport-only hover does not need high FPS when nothing is animating.

### macOS Timer Coalescing

macOS coalesces timers to save power, which can cause `glfwWaitEventsTimeout` to wake slightly late (e.g., 20ms instead of 16ms). This is fine for an editor -- the timeout is a maximum, not a precision target. Any event (mouse move, key press) wakes immediately regardless.

### windowRefreshCallback Compatibility

The existing `windowRefreshCallback` (line 85) calls `renderFrame()` during macOS live resize. This is compatible with `glfwWaitEventsTimeout` because:
- During resize, GLFW generates continuous events, so `WaitEventsTimeout` returns immediately
- The refresh callback fires within `glfwWaitEventsTimeout` itself (it processes events before returning)

### deltaTime Compatibility

The editor uses `1.0f / std::max(ImGui::GetIO().Framerate, 1.0f)` for deltaTime. When idle FPS drops to ~15, deltaTime becomes ~0.067s. This is fine because:
- Camera animation will force continuous rendering (60fps) while active
- Environment panel tweaks happen during interaction (which triggers continuous rendering)
- No physics simulation in the editor

## Common Pitfalls

### Pitfall 1: Forgetting to Poll During Animations
**What goes wrong:** Camera focus animation (F key) runs at 15 FPS, looks stuttery.
**How to avoid:** Check `cameraAnim.active` in the needsContinuousRendering flag.

### Pitfall 2: Using glfwPollEvents AND glfwWaitEventsTimeout Together
**What goes wrong:** Calling both in the same frame processes events twice, causing erratic timing.
**How to avoid:** Use exactly one event function per frame iteration. The conditional should be mutually exclusive.
Source: [GLFW discourse](https://discourse.glfw.org/t/when-using-glfwwaiteventstimeout-to-cap-framerate-its-somehow-oddly-throttled-unless-i-move-my-mouse-all-the-time/2277)

### Pitfall 3: Hot-Reload Stops Working
**What goes wrong:** Material hot-reload poll (line 423) runs less often, feels delayed.
**How to avoid:** The 500ms hot-reload interval is already infrequent. With ~15 FPS idle, it fires every ~33ms -- still more than enough. No change needed.

### Pitfall 4: ImGui Cursor Blink Stops
**What goes wrong:** Text input cursor stops blinking when no events arrive.
**How to avoid:** The 15 FPS idle timeout (67ms) is well under the typical blink period (530ms). Cursor will blink correctly at reduced rate.

### Pitfall 5: glfwPostEmptyEvent Race on X11
**What goes wrong:** On Linux/X11, `glfwPostEmptyEvent` historically had a race condition where it could fail to wake `glfwWaitEvents`.
**How to avoid:** Fixed in modern GLFW 3.4 via pipe-based signaling. Not a concern on macOS. Only relevant if cross-thread wake is needed later.
Source: [GLFW PR #2033](https://github.com/glfw/glfw/pull/2033)

## Sources

### Primary (HIGH confidence)
- [ImGui Wiki: Implementing Power Save](https://github.com/ocornut/imgui/wiki/Implementing-Power-Save,-aka-Idling-outside-of-ImGui) -- official ImGui recommendation for idle throttling
- [GLFW 3.4 Input Guide](https://www.glfw.org/docs/3.4/input_guide.html) -- official docs for PollEvents/WaitEvents/WaitEventsTimeout
- Project source: `apps/level_editor/main.cpp` -- current implementation analyzed directly

### Secondary (MEDIUM confidence)
- [ImGui issue #5023](https://github.com/ocornut/imgui/issues/5023) -- differential throttling discussion
- [ImGui issue #4133](https://github.com/ocornut/imgui/issues/4133) -- wait-event-timeout main loop modification
- [ImGui issue #6308](https://github.com/ocornut/imgui/issues/6308) -- high CPU usage discussion
- [GLFW issue #1303](https://github.com/glfw/glfw/issues/1303) -- high CPU load with glfwPollEvents
- [GLFW vsync issues](https://github.com/glfw/glfw/issues/1990) -- macOS vsync unreliability

### Tertiary (LOW confidence)
- Blender idle loop behavior -- inferred from issue tracker discussions, not direct source reading

## Metadata

**Confidence breakdown:**
- Event function selection: HIGH -- GLFW docs + ImGui wiki are authoritative
- Activity detection flags: HIGH -- read directly from editor source code
- macOS vsync issues: MEDIUM -- multiple GLFW issues confirm, but current macOS Sequoia status unknown
- Timer coalescing impact: MEDIUM -- documented macOS behavior, not tested with this specific app

**Research date:** 2026-04-01
**Valid until:** 2026-05-01 (stable APIs, unlikely to change)
