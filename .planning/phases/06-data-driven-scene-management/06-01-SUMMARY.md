---
phase: 06-data-driven-scene-management
plan: 01
subsystem: build
tags: [cmake, mesh-library, asset-registration, project-config]

# Dependency graph
requires:
  - phase: 05-unify-editor-runtime-build-rendering-parity
    provides: SceneRenderPipeline, EditorPreviewWorld, RuntimeGameSession structure
provides:
  - registerAllGameAssets() — single consolidated asset registration function for all call sites
  - readProjectCfgLastScene() / writeProjectCfgLastScene() — project.cfg I/O layer
  - Deleted legacy CathedralScene, SilosCloisterScene, WardenOfficeScene scene classes
  - Deleted legacy CathedralAssets and PrisonAssets per-level files
affects:
  - 06-02 (uses readProjectCfgLastScene to load last_scene at runtime and editor startup)
  - Any future level addition (adds meshes to GameAssets.cpp only, one callsite)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Single consolidated registerAllGameAssets() replacing scattered per-level registration calls
    - project.cfg key=value line-based format with rfind prefix matching (no sscanf, string values)

key-files:
  created:
    - src/game/levels/GameAssets.h
    - src/game/levels/GameAssets.cpp
    - src/engine/core/ProjectConfig.h
    - src/engine/core/ProjectConfig.cpp
  modified:
    - src/game/CMakeLists.txt
    - src/engine/CMakeLists.txt
    - src/game/scenes/GenericFileScene.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - apps/model_viewer/main.cpp
    - apps/runtime/main.cpp

key-decisions:
  - "registerAllGameAssets() calls registerDefaults() exactly once (pitfall: both cathedral and prison each called it — deduplication required)"
  - "project.cfg stores bare scene filename only (e.g. warden_office.scene), callers prepend assets/scenes/ path via resolveProjectPath()"
  - "runtime main.cpp WardenOfficeScene fallback replaced with GenericFileScene('assets/scenes/warden_office.scene') as temporary bridge before Plan 02 adds project.cfg reading"
  - "rfind prefix matching (not sscanf) used in ProjectConfig since value is a string not a number — pattern mirrors level_editor loadWindowGeometry but adapted for string values"

patterns-established:
  - "registerAllGameAssets: one function, one call site pattern — add new mesh types here only"
  - "project.cfg: key=value text file, one entry per line, last_scene key is bare filename"

requirements-completed: []

# Metrics
duration: 5min
completed: 2026-03-30
---

# Phase 06 Plan 01: Asset Consolidation and ProjectConfig Summary

**Merged cathedral and prison mesh registration into single registerAllGameAssets(), deleted 10 legacy files, and added project.cfg read/write utilities to engine_core**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-30T18:30:02Z
- **Completed:** 2026-03-30T18:35:00Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- Created `registerAllGameAssets()` consolidating all 14 mesh registrations with exactly one `registerDefaults()` call
- Updated 4 surviving call sites (GenericFileScene, EditorPreviewWorld, RuntimeGameSession, model_viewer) to use the unified function
- Deleted 10 legacy files: 6 scene class files (Cathedral/SilosCloister/WardenOffice .h/.cpp) and 4 asset files (CathedralAssets/PrisonAssets .h/.cpp)
- Replaced hardcoded `WardenOfficeScene` fallback in runtime main with `GenericFileScene` path-based scene loading
- Created `readProjectCfgLastScene()` and `writeProjectCfgLastScene()` in engine_core for Plan 02's startup scene I/O

## Task Commits

1. **Task 1: Consolidate asset registration, delete legacy scenes and asset files** — `e0661f3` (feat)
2. **Task 2: Create ProjectConfig utilities for project.cfg read/write** — `79ee193` (feat)

**Plan metadata:** (included in final docs commit)

## Files Created/Modified

- `src/game/levels/GameAssets.h` — declares `void registerAllGameAssets(MeshLibrary& meshLibrary)`
- `src/game/levels/GameAssets.cpp` — merged cathedral + prison registrations, single `registerDefaults()` call
- `src/engine/core/ProjectConfig.h` — declares `readProjectCfgLastScene` and `writeProjectCfgLastScene`
- `src/engine/core/ProjectConfig.cpp` — key=value parsing with rfind prefix match; trunc-writes entire file
- `src/game/CMakeLists.txt` — game_content: `GameAssets.cpp` replaces two per-level files; gameplay: 3 legacy scenes removed
- `src/engine/CMakeLists.txt` — engine_core: added `core/ProjectConfig.cpp`
- `src/game/scenes/GenericFileScene.cpp` — uses `registerAllGameAssets`
- `src/editor/scene/EditorPreviewWorld.cpp` — uses `registerAllGameAssets`
- `src/game/runtime/RuntimeGameSession.cpp` — uses `registerAllGameAssets`
- `apps/model_viewer/main.cpp` — uses `registerAllGameAssets`
- `apps/runtime/main.cpp` — removed legacy scene includes; fallback uses `GenericFileScene(path)` not `WardenOfficeScene`

## Decisions Made

- `registerAllGameAssets()` calls `registerDefaults()` exactly once — both CathedralAssets.cpp and PrisonAssets.cpp each called it separately; merged file keeps only one at the top
- project.cfg stores bare filename (`warden_office.scene`), not a full path — avoids working-directory pitfalls documented in RESEARCH.md
- Used `rfind("last_scene=", 0)` prefix matching (not `sscanf`) since the value is a string — mirrors `loadWindowGeometry` pattern from level_editor/main.cpp but adapted for string output

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. All three executables (pixel-roguelike, level-editor, procedural-model-viewer) compiled cleanly on first build attempt after changes.

## Known Stubs

None - all new code is fully functional. The `GenericFileScene("assets/scenes/warden_office.scene")` fallback in apps/runtime/main.cpp is intentional as a temporary bridge until Plan 02 wires project.cfg reading.

## Next Phase Readiness

- Plan 02 can now call `readProjectCfgLastScene(cfgPath)` from engine_core without circular dependencies
- All mesh assets accessible from a single include: `#include "game/levels/GameAssets.h"`
- Legacy scene class types fully removed — no lingering references anywhere in src/ or apps/

---
*Phase: 06-data-driven-scene-management*
*Completed: 2026-03-30*
