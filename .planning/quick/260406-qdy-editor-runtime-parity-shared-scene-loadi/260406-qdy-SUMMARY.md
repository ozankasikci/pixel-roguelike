---
phase: quick-260406-qdy
plan: 01
subsystem: editor/game-scenes
tags: [editor-parity, scripted-geometry, preview-session, level-loading]
dependency_graph:
  requires: []
  provides: [editor-runtime-parity-scripted-geometry]
  affects: [EditorRuntimePreviewSession, GenericFileScene, InitialSceneScripted]
tech_stack:
  added: []
  patterns: [static-registry-lookup, stem-based-levelId-derivation]
key_files:
  created:
    - src/game/scenes/InitialSceneScripted.h
  modified:
    - src/game/scenes/GenericFileScene.h
    - src/game/scenes/GenericFileScene.cpp
    - src/editor/core/EditorRuntimePreviewSession.cpp
    - apps/level_editor/main.cpp
    - apps/runtime/main.cpp
    - src/game/scenes/InitialSceneScripted.cpp
decisions:
  - Scripted geometry runs in preview session only, not the outliner/scene document
  - Scripted geometry entities are editable during preview but non-persistent (reset on restart)
  - LevelId derived from scene path stem to match GenericFileScene constructor behaviour
metrics:
  duration: ~8 minutes
  completed: 2026-04-06T16:09:58Z
  tasks_completed: 2
  files_changed: 6
---

# Quick Task 260406-qdy: Editor-Runtime Parity — Shared Scene Loading with Scripted Geometry

**One-liner:** Editor preview session now runs full runtime scene loading including scripted geometry doors via GenericFileScene's static registry.

## What Was Built

The editor preview session was missing scripted geometry (doors, code-driven entities) that only the game runtime spawned. This caused the editor and runtime to look different for `initial_scene`, making level design unreliable.

Three changes fix the gap:

1. **`GenericFileScene::lookupScriptedGeometry()`** — New public static method exposing read access to the file-local scripted geometry registry. Mirrors the existing private lookup in `onEnter()` but available to external callers like the editor.

2. **`EditorRuntimePreviewSession::rebuild()`** — Now derives `lookupId` from the scene path stem (matching the `GenericFileScene` constructor pattern) and populates `request.buildScriptedGeometry` from the registry before passing to `RuntimeGameSession::rebuild()`.

3. **`apps/level_editor/main.cpp`** — Calls `registerInitialSceneScripted()` at startup (mirroring `runtime/main.cpp`) so the registry is populated before any preview session loads a scene.

The prerequisite refactor (`InitialSceneScripted` extracted from anonymous static initializer to a named free function) was also committed as it was staged but uncommitted.

## Commits

| Hash | Description |
|------|-------------|
| 0f64327 | Add public lookupScriptedGeometry() to GenericFileScene registry |
| 1ddbc0e | Wire editor preview session to run scripted geometry for full runtime parity |
| 1914310 | Refactor InitialSceneScripted to a named function and call it from runtime main |

## Deviations from Plan

**1. [Rule 2 - Missing critical prerequisite] Committed staged InitialSceneScripted refactor**
- **Found during:** Task 2 — `apps/runtime/main.cpp`, `InitialSceneScripted.cpp`, and new `InitialSceneScripted.h` were modified/untracked but uncommitted from the base commit
- **Issue:** The plan depends on `registerInitialSceneScripted()` being a named function callable from both runtime and editor, but the refactor that created this function had not been committed
- **Fix:** Committed the three staged files as a prerequisite commit
- **Files modified:** `src/game/scenes/InitialSceneScripted.h` (new), `src/game/scenes/InitialSceneScripted.cpp`, `apps/runtime/main.cpp`
- **Commit:** 1914310

## Known Stubs

None — all wired to the live registry.

## Self-Check

### Files exist:
- `src/game/scenes/GenericFileScene.h` — contains `lookupScriptedGeometry`
- `src/game/scenes/GenericFileScene.cpp` — contains `lookupScriptedGeometry` implementation
- `src/editor/core/EditorRuntimePreviewSession.cpp` — contains `lookupScriptedGeometry` call in `rebuild()`
- `apps/level_editor/main.cpp` — contains `registerInitialSceneScripted()` call
- `src/game/scenes/InitialSceneScripted.h` — new header with function declaration

### Commits exist:
- 0f64327, 1ddbc0e, 1914310 — all confirmed in git log

## Self-Check: PASSED
