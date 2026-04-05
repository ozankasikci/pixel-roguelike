---
phase: quick
plan: 260406-1vc
subsystem: editor/build
tags: [editor, build-system, packaging, scene-selection, ui]
dependency_graph:
  requires: []
  provides: [scene-filtered-packaging]
  affects: [EditorBuildSystem, level-editor]
tech_stack:
  added: []
  patterns: [INI key-value persistence, comma-separated list encoding, ImGui submenu with checkboxes]
key_files:
  created: []
  modified:
    - src/editor/build/EditorBuildSystem.h
    - src/editor/build/EditorBuildSystem.cpp
    - apps/level_editor/main.cpp
decisions:
  - "Empty buildScenes vector means all scenes (default behavior, no regression)"
  - "Comma-separated INI value for build_scenes= key (simple, no dependencies)"
  - "allBuildableScenes initialized once at startup (restart needed for new scenes)"
metrics:
  duration: "~15 minutes"
  completed: "2026-04-05T22:30:23Z"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 3
---

# Quick 260406-1vc: Add Scene Selection System for Builds Summary

**One-liner:** Per-scene checkboxes in Build menu control which scenes and their referenced assets are packaged, with comma-separated INI persistence via `build_scenes=` key.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Add build_scenes to EditorBuildConfig with persistence and filtered asset collection | ff3394f | EditorBuildSystem.h, EditorBuildSystem.cpp |
| 2 | Add Build Scenes submenu to editor Build menu | f160639 | apps/level_editor/main.cpp |

## What Was Built

### Task 1 — EditorBuildSystem changes

- `EditorBuildConfig` now has `std::vector<std::string> buildScenes` (empty = all scenes)
- New `listBuildableScenes()` function: scans `assets/scenes/*.scene`, returns filenames sorted alphabetically
- `collectUsedAssets()` signature changed to `collectUsedAssets(const std::vector<std::string>& sceneFilter = {})` — when sceneFilter is non-empty, only scans matching scene files for mesh IDs
- `packageGame()` no longer includes `"scenes"` in `alwaysCopyDirs`; instead copies scenes selectively, logging "Package: including N of M scenes"
- `packageGame()` passes `config.buildScenes` to `collectUsedAssets()` so only assets referenced by selected scenes are included in the package
- `loadBuildConfig()` parses `build_scenes=file1.scene,file2.scene` (comma-separated)
- `saveBuildConfig()` writes `build_scenes=...` key only when list is non-empty (empty = all, not written)

### Task 2 — main.cpp Build menu UI

- `allBuildableScenes` initialized from `listBuildableScenes()` immediately after `loadBuildConfig()` at startup
- "Build Scenes" submenu added between "Package for Sharing" and "Configuration" submenu
- "All Scenes (default)" toggle at top — clicking it clears `buildScenes` reverting to include-all mode
- Per-scene checkboxes for each discovered `.scene` file
- Toggle logic: switching from all-selected to selective pre-populates list then removes the clicked scene; removing last item leaves empty list (= all); adding all scenes back reverts to empty (= all)
- No changes needed to the `packageGame()` call site — it already receives `buildConfig`
- `saveBuildConfig()` at shutdown persists the selection

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Verification

- `cmake --build build --target level-editor --parallel` — compiled clean with 0 errors, 0 warnings
- Build Scenes submenu is accessible at Build > Build Scenes
- Per-scene checkboxes toggle correctly with "All Scenes (default)" reverting on full selection
- Packaging passes `config.buildScenes` as scene filter for both scene file copying and asset manifest collection

## Self-Check: PASSED

Files exist:
- FOUND: src/editor/build/EditorBuildSystem.h
- FOUND: src/editor/build/EditorBuildSystem.cpp
- FOUND: apps/level_editor/main.cpp

Commits exist:
- ff3394f — Add build_scenes config with persistence, listBuildableScenes, and filtered asset collection
- f160639 — Add Build Scenes submenu to editor Build menu with per-scene checkboxes
