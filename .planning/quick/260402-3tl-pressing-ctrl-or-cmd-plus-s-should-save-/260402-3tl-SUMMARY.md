---
phase: quick
plan: 260402-3tl
subsystem: editor
tags: [keyboard-shortcut, save, editor-ux]
dependency_graph:
  requires: []
  provides: [Ctrl/Cmd+S save shortcut in level editor]
  affects: [apps/level_editor/main.cpp]
tech_stack:
  added: []
  patterns: [ImGui global shortcut handler with KeyCtrl/KeySuper modifier]
key_files:
  created: []
  modified:
    - apps/level_editor/main.cpp
decisions:
  - No WantTextInput guard needed for Ctrl+S — matches Ctrl+D pattern since S never produces text input in this context
metrics:
  duration: "2min"
  completed: "2026-04-02"
  tasks: 1
  files: 1
---

# Quick Task 260402-3tl: Ctrl/Cmd+S Save Shortcut Summary

**One-liner:** Wired Ctrl+S (Windows/Linux) and Cmd+S (macOS) to the existing `savePressed` flag in the level editor global shortcut handler.

## What Was Done

Added one line to `apps/level_editor/main.cpp` inside the `!gameplayPreviewCaptured` shortcut block, after the Ctrl+D duplicate handler, following the identical pattern used by all other global editor shortcuts:

```cpp
if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S)) savePressed = true;
```

The `savePressed` flag already drove save execution at line 1796 (`document.save(content)`, `commandStack.markSaved(document)`). The File > Save menu item already displayed "Ctrl/Cmd+S" as a hint label. This one line closes the gap between the UI hint and the actual behavior.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Add Ctrl/Cmd+S keyboard shortcut for scene save | 5cd4b53 | apps/level_editor/main.cpp |

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED

- `apps/level_editor/main.cpp` modified with `ImGuiKey_S` shortcut: confirmed
- Commit `5cd4b53` exists: confirmed
- Build succeeds with no errors: confirmed (all targets built successfully)
