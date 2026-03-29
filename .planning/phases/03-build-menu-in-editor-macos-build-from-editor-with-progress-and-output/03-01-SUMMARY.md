---
phase: 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output
plan: 01
subsystem: editor
tags: [posix, fork, exec, pipe, sigterm, imgui, cmake, scene-loading]

requires:
  - phase: 01.1-project-restructure-ecs-application-class-modular-engine-game-split
    provides: LevelLoader, LevelBuildContext, SceneManager, Scene base class

provides:
  - EditorBuildSystem backend module (fork/exec cmake, pipe capture, SIGTERM cancellation, progress parsing)
  - GenericFileScene for loading arbitrary .scene files by path
  - --scene <path> CLI argument for the runtime executable

affects:
  - 03-02 (Plan 02 wires EditorBuildSystem into editor UI build menu)

tech-stack:
  added: []
  patterns:
    - "fork()/execvp() with setpgid(0,0) for cmake process spawning with process group SIGTERM support"
    - "Non-blocking pipe with dedicated reader thread and mutex-protected line queue for streaming output"
    - "ImGuiTextBuffer + ImVector<int> lineOffsets (ExampleAppLog pattern) for per-line colored build output"
    - "Lock-swap pattern for thread-safe queue drain: swap under lock, process outside lock"
    - "GenericFileScene mirrors SilosCloisterScene pattern — same onEnter/onExit structure, path-parameterized"

key-files:
  created:
    - src/editor/build/EditorBuildSystem.h
    - src/editor/build/EditorBuildSystem.cpp
    - src/game/scenes/GenericFileScene.h
    - src/game/scenes/GenericFileScene.cpp
  modified:
    - src/editor/CMakeLists.txt
    - src/game/CMakeLists.txt
    - apps/runtime/main.cpp

key-decisions:
  - "src/editor/build/ directory ignored by .gitignore (build/ pattern) — files added via git add -f to bypass"
  - "Worktree build requires FETCHCONTENT_BASE_DIR pointing to main project deps to avoid re-fetching assimp/GLAD"
  - "GenericFileScene uses registerCathedralAssets for all scenes — all current scenes use cathedral asset set"
  - "loadBuildConfig/saveBuildConfig use separate editor_build.ini-style file, not merged into editor_window.ini"

patterns-established:
  - "Process group cancellation: child calls setpgid(0,0) before execvp; parent uses kill(-pid, SIGTERM)"
  - "Reader thread drain: vector::swap under mutex lock, then process swapped-out lines without holding lock"

requirements-completed: []

duration: 90min
completed: 2026-03-29
---

# Phase 03 Plan 01: EditorBuildSystem Backend and --scene Argument Summary

**POSIX fork/exec cmake process manager with pipe-based output streaming, SIGTERM cancellation, and GenericFileScene for --scene runtime argument**

## Performance

- **Duration:** ~90 min
- **Started:** 2026-03-29T08:00:00Z
- **Completed:** 2026-03-29T09:46:16Z
- **Tasks:** 2
- **Files modified:** 7 (4 created, 3 modified)

## Accomplishments
- EditorBuildSystem backend module with full POSIX process lifecycle: startBuild, startConfigure, cancelBuild, pollBuild, launchGame
- Non-blocking output capture via merged stdout+stderr pipe and dedicated reader thread with mutex queue
- CMake progress line parsing via sscanf for [XX%] percentage extraction (D-10)
- SIGTERM cancellation to full process group via kill(-pid, SIGTERM) (D-14)
- GenericFileScene that loads any .scene file by path, following the SilosCloisterScene pattern exactly
- Runtime executable --scene argument that pushes GenericFileScene when provided (D-18)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create EditorBuildSystem backend module** - `ebb4257` (feat)
2. **Task 2: Add --scene argument to runtime executable** - `a30ff8c` (feat)

## Files Created/Modified
- `src/editor/build/EditorBuildSystem.h` - BuildLineKind, BuildOutputLog, EditorBuildConfig, EditorBuildState structs and free function declarations
- `src/editor/build/EditorBuildSystem.cpp` - POSIX process spawning, reader thread, progress parsing, SIGTERM cancellation, INI persistence
- `src/editor/CMakeLists.txt` - Added build/EditorBuildSystem.cpp to editor static library
- `src/game/scenes/GenericFileScene.h` - Scene subclass that loads .scene file by path
- `src/game/scenes/GenericFileScene.cpp` - onEnter/onExit mirroring SilosCloisterScene, path-parameterized via LevelLoader
- `src/game/CMakeLists.txt` - Added scenes/GenericFileScene.cpp to gameplay library
- `apps/runtime/main.cpp` - Added --scene argument parsing; pushes GenericFileScene when provided

## Decisions Made
- `src/editor/build/` directory is ignored by root `.gitignore` (the `build/` glob pattern matches all dirs named "build"). Files were added using `git add -f` to bypass the ignore. This is expected behavior in this project structure.
- Worktree build required `FETCHCONTENT_BASE_DIR=/path/to/main/project/build/_deps` and a venv Python for GLAD code generation, since worktree creates a new build directory from scratch.
- GenericFileScene registers cathedral assets for all scenes (`registerCathedralAssets`) — consistent with the current state of the project where all scenes use the cathedral asset set.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- **Git worktree build setup**: The git worktree has a separate build directory that needed `FETCHCONTENT_BASE_DIR` pointed at the main project's `build/_deps` to avoid re-downloading assimp (large). GLAD code generation also required the project's `.venv/bin/python3` (which has jinja2) via `-DPython_EXECUTABLE=...`. These are infrastructure-only issues, not code issues.
- **gitignore conflict**: `build/` in root `.gitignore` was matching `src/editor/build/`. Resolved by `git add -f`.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plan 02 can now include `editor/build/EditorBuildSystem.h` and call `startBuild()`, `pollBuild()`, `cancelBuild()`, `launchGame()` directly
- Runtime executable accepts `--scene assets/scenes/silos_cloister.scene` for testing
- All structs (BuildOutputLog, EditorBuildState, EditorBuildConfig) are defined and ready for Plan 02 UI integration

## Self-Check: PASSED

All files verified:
- FOUND: src/editor/build/EditorBuildSystem.h
- FOUND: src/editor/build/EditorBuildSystem.cpp
- FOUND: src/game/scenes/GenericFileScene.h
- FOUND: src/game/scenes/GenericFileScene.cpp
- FOUND: commit ebb4257 (Task 1)
- FOUND: commit a30ff8c (Task 2)

---
*Phase: 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output*
*Completed: 2026-03-29*
