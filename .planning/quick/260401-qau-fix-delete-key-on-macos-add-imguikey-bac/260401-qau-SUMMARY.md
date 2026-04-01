---
phase: quick
plan: 260401-qau
subsystem: editor
tags: [input, keyboard, macos, imgui]
dependency_graph:
  requires: []
  provides: [macos-delete-key-works]
  affects: [apps/level_editor/main.cpp]
tech_stack:
  added: []
  patterns: [ImGuiKey_Backspace as macOS delete alias]
key_files:
  created: []
  modified:
    - apps/level_editor/main.cpp
decisions:
  - "Added !io.WantTextInput guard to the delete handler since original code lacked it — ensures Backspace in text fields remains unaffected"
metrics:
  duration: 3m
  completed: 2026-04-01
  tasks_completed: 1
  files_changed: 1
---

# Quick 260401-qau: Fix Delete Key on macOS Summary

**One-liner:** Added ImGuiKey_Backspace as alias for ImGuiKey_Delete so the macOS labeled-delete key triggers object deletion in the level editor.

## What Was Done

Task 1 updated `apps/level_editor/main.cpp` in two places:

1. Shortcut handler (line 432): Changed `ImGui::IsKeyPressed(ImGuiKey_Delete)` to `!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))` — both keys now trigger deletion, text input fields remain unaffected.

2. Edit menu item (line 552): Updated shortcut hint label from `"Delete"` to `"Delete/Backspace"` so the UI reflects the accepted keys.

## Deviations from Plan

**1. [Rule 2 - Missing critical functionality] Added !io.WantTextInput guard to the delete shortcut handler**
- **Found during:** Task 1
- **Issue:** The original line `if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;` had no `!io.WantTextInput` guard. Adding `ImGuiKey_Backspace` without this guard would cause Backspace to delete objects while typing in text fields.
- **Fix:** Added `!io.WantTextInput` guard alongside the Backspace addition.
- **Files modified:** apps/level_editor/main.cpp
- **Commit:** 698ccf3

## Commits

| Hash | Description |
|------|-------------|
| 698ccf3 | fix delete key on macOS by adding ImGuiKey_Backspace as alternative trigger |

## Self-Check: PASSED

- `apps/level_editor/main.cpp` modified: FOUND
- Commit 698ccf3: FOUND
- Build target level-editor: PASSED (100% Built target level-editor)
