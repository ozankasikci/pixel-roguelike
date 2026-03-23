# Input System Design

## Overview

Replace the minimal polling-based InputSystem with a callback-driven input system that tracks keyboard, mouse buttons, mouse movement, and scroll wheel with per-frame edge detection.

## Design Decisions

- **Classic FPS mouse look**: cursor locked and hidden, raw mouse deltas control camera
- **Hardcoded key bindings**: no action-mapping layer, systems check GLFW keys directly
- **Mouse buttons**: left + right tracked (primary/secondary action)
- **Scroll wheel**: vertical delta tracked per frame
- **Edge detection**: "just pressed" / "just released" for both keys and mouse buttons
- **InputSystem is a dumb data provider**: raw state only, no camera logic. CameraSystem interprets the data.

## State Tracked Per Frame

- Key states: current frame + previous frame arrays (indexed by GLFW key codes, max ~512)
- Mouse button states: current + previous for left, right, middle (3 buttons)
- Mouse position: current X/Y
- Mouse delta: movement since last frame (accumulated from callback, reset each frame)
- Scroll delta: vertical scroll since last frame (accumulated, reset each frame)
- Cursor locked flag

## Public API

```cpp
// Keyboard
bool isKeyPressed(int key) const;      // held this frame
bool isKeyJustPressed(int key) const;  // pressed this frame, not last
bool isKeyJustReleased(int key) const; // released this frame, was pressed last

// Mouse buttons
bool isMouseButtonPressed(int button) const;
bool isMouseButtonJustPressed(int button) const;
bool isMouseButtonJustReleased(int button) const;

// Mouse movement
glm::vec2 mouseDelta() const;
glm::vec2 mousePosition() const;
float scrollDelta() const;

// Cursor control
void lockCursor();
void unlockCursor();
bool isCursorLocked() const;
```

## GLFW Callback Wiring

InputSystem stores a static pointer to itself (standard GLFW pattern for C-style callbacks). Registered in `init()`:

- `glfwSetKeyCallback` — updates key state array
- `glfwSetMouseButtonCallback` — updates button state array
- `glfwSetCursorPosCallback` — accumulates mouse delta
- `glfwSetScrollCallback` — accumulates scroll delta

## Frame Lifecycle (`update()`)

1. Copy current key/button arrays to previous arrays
2. Process accumulated deltas (mouse movement, scroll)
3. Reset accumulators for next frame

## Cursor Lock

- Uses `glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED)`
- Enables `glfwSetRawMouseMotion` if supported (avoids OS acceleration)
- Cursor starts locked on init
- ImGui capture check remains: when cursor is unlocked, ImGui gets mouse priority

## Impact on CameraSystem

- Drops all direct GLFW includes
- Reads `input_.mouseDelta()` for camera look instead of arrow keys
- Applies sensitivity multiplier to yaw/pitch from mouse deltas
- WASD movement uses `isKeyPressed()` backed by callback state instead of `glfwGetKey()`
- Arrow key look removed — mouse is the look input

## Files Changed

- `src/engine/input/InputSystem.h` — rewritten
- `src/engine/input/InputSystem.cpp` — rewritten
- `src/game/systems/CameraSystem.cpp` — updated to use mouse look
