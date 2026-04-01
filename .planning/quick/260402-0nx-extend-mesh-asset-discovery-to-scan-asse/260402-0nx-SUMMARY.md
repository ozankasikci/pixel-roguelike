---
phase: quick
plan: 260402-0nx
subsystem: editor, game/runtime, game/scenes
tags: [mesh-discovery, asset-browser, packs]
dependency_graph:
  requires: []
  provides: [assets/packs mesh discovery in editor, runtime, and GenericFileScene]
  affects: [EditorPreviewWorld, RuntimeGameSession, GenericFileScene]
tech_stack:
  added: []
  patterns: [second discoverProjectAssets call with assets/packs root]
key_files:
  modified:
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/game/scenes/GenericFileScene.cpp
decisions:
  - assets/meshes/ scanned first so its IDs take priority over identically-named pack meshes
  - has() guard on all three sites prevents duplicate registration
  - discoverProjectAssets gracefully handles missing directory (returns empty vector)
metrics:
  duration: 5m
  completed: 2026-04-02
  tasks_completed: 1
  files_modified: 3
---

# Phase quick Plan 260402-0nx: Extend Mesh Asset Discovery to Scan assets/packs/ Summary

**One-liner:** Added second `discoverProjectAssets` call for `assets/packs/` to all three mesh discovery sites so FBX/glb files from downloaded asset packs appear in the editor browser and load at runtime.

## Tasks Completed

| # | Name | Commit | Files |
|---|------|--------|-------|
| 1 | Add assets/packs/ scanning to all three mesh discovery call sites | 24dcba0 | EditorPreviewWorld.cpp, RuntimeGameSession.cpp, GenericFileScene.cpp |

## What Was Done

Each of the three call sites received an additional scan loop immediately after the existing `assets/meshes/` loop:

- **`EditorPreviewWorld.cpp`** — `registerDiscoveredMeshAssets()`: editor asset browser now discovers meshes from `assets/packs/` and all its subdirectories.
- **`RuntimeGameSession.cpp`** — `bootstrapRuntimeMeshLibrary()`: runtime game sessions discover and load pack meshes on startup.
- **`GenericFileScene.cpp`** — `registerAssets` lambda: scene load path discovers pack meshes before building the level.

The pattern is identical at all three sites: call `ModelLoader::discoverProjectAssets` with the packs directory, then register any mesh ID not already present (guarded by `has()`). `discoverProjectAssets` uses `recursive_directory_iterator` internally, so nested subdirectories like `DoorPackFree/`, `Free Wood Door Pack/`, and `QuestDoorsPack/` are automatically traversed.

## Deviations from Plan

None — plan executed exactly as written.

## Verification

Build passed: `cmake --build build --target pixel-roguelike level-editor` completed with no errors.

## Self-Check: PASSED

- `src/editor/scene/EditorPreviewWorld.cpp` — modified, contains `assets/packs`
- `src/game/runtime/RuntimeGameSession.cpp` — modified, contains `assets/packs`
- `src/game/scenes/GenericFileScene.cpp` — modified, contains `assets/packs`
- Commit `24dcba0` exists in git log
