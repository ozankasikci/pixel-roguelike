---
phase: 06-data-driven-scene-management
plan: 03
subsystem: editor
tags: [editor-ui, scene-management, imgui, asset-browser, unsaved-changes]

# Dependency graph
requires:
  - phase: 06-data-driven-scene-management
    plan: 01
    provides: readProjectCfgLastScene / writeProjectCfgLastScene utilities
  - phase: 06-data-driven-scene-management
    plan: 02
    provides: ui.pendingScenePath empty-state sentinel, writeProjectCfgLastScene on scene open

provides:
  - New Scene creation from File menu and asset browser plus button
  - Delete Scene with confirmation modal and active-scene close
  - Inline Rename Scene with project.cfg and editor state update
  - Active scene highlight (green text + [active] badge) in asset browser
  - Unsaved changes guard modal on scene switch (Save / Don't Save / Cancel)

affects:
  - Editor users: complete scene lifecycle management from editor UI

# Tech tracking
tech-stack:
  added: []
  patterns:
    - doLoadScene helper lambda to DRY up loadSceneIntoEditor across three call sites (direct, Save path, Don't Save path)
    - SceneRenameState static singleton for inline rename lifecycle management
    - Modals opened outside root editor window (same pattern as Save Before Building?)

key-files:
  created: []
  modified:
    - src/editor/ui/LevelEditorUi.h
    - src/editor/ui/EditorAssetBrowserPanel.cpp
    - apps/level_editor/main.cpp

key-decisions:
  - "doLoadScene lambda defined inline in renderFrame to avoid repeating 15-line loadSceneIntoEditor call across Save/Don't Save/direct paths"
  - "NewScenePopup and DeleteSceneConfirm modals placed outside root window (after assetBrowserActions populated), matching Save Before Building? pattern"
  - "SceneRenameState uses static singleton (same pattern as AssetBrowserSession) — file-scope state persists across frames without heap allocation"
  - "Inline rename cancels on click-away using IsMouseClicked guard after IsItemActive check"

patterns-established:
  - "Modals triggered by both menu items (inside root window) and asset browser actions (outside root window) use a bool flag approach — flag set in both triggers, OpenPopup called once outside root window"

requirements-completed: []

# Metrics
duration: 15min
completed: 2026-03-30
---

# Phase 06 Plan 03: Scene Management Editor Features Summary

**New Scene creation (File menu + asset browser plus button), Delete/Rename scene ops with confirmations, active scene highlight, and unsaved-changes guard on scene switch**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-03-30T18:50:00Z
- **Completed:** 2026-03-30
- **Tasks:** 1 of 2 (Task 2 is human verification checkpoint)
- **Files modified:** 3

## Accomplishments

- Added `newSceneRequested`, `deleteScenePath`, `renameScenePath` fields to `AssetBrowserActionResult` in `LevelEditorUi.h`
- Added `SceneRenameState` struct and singleton accessor in `EditorAssetBrowserPanel.cpp` anonymous namespace
- Implemented active scene highlight: green text `ImVec4(0.4f, 0.8f, 0.4f, 1.0f)` + `[active]` text badge for currently open scene
- Implemented inline rename for Scene nodes: `InputText` replaces `Selectable` when renaming; commits on Enter, cancels on click-away; updates `project.cfg` and `document.setScenePath` for active scene
- Extended Scene context menu: Open Scene, Rename, Separator, Delete...
- Added `+##NewScene` SmallButton next to "scenes" folder in tree
- Added `File > New Scene...` menu item
- Added `NewScenePopup` modal: creates minimal LevelDef (environmentId="default", hasPlayerSpawn=true) in assets/scenes/, handles name collisions, refreshes scene paths
- Added `DeleteSceneConfirm` modal: removes file, clears document + ui.pendingScenePath if active scene deleted, clears project.cfg if last_scene matches
- Replaced plain `requestedScenePath` handler with `doLoadScene` lambda + unsaved changes guard: `document.dirty()` check gates immediate load vs modal
- Added `UnsavedChangesOnSwitch` modal: Save / Don't Save / Cancel with consistent doLoadScene invocation
- Wired empty-state "New Scene..." button to `newScenePopupRequested` flag (replaces Plan 02 placeholder popup)

## Task Commits

1. **Task 1: New Scene creation, Delete/Rename scene operations, asset browser enhancements, unsaved changes guard** — `3b70236`

## Files Created/Modified

- `src/editor/ui/LevelEditorUi.h` — Added `newSceneRequested`, `deleteScenePath`, `renameScenePath` to `AssetBrowserActionResult`
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — SceneRenameState, active highlight, inline rename, extended context menu, plus button
- `apps/level_editor/main.cpp` — NewScenePopup, DeleteSceneConfirm, UnsavedChangesOnSwitch modals; doLoadScene lambda; File > New Scene... menu item; state variables

## Decisions Made

- `doLoadScene` lambda defined inline in `renderFrame` to avoid repeating the 15-line `loadSceneIntoEditor` call across Save, Don't Save, and direct load paths — single source of truth for scene loading side effects
- Modals placed outside root editor window (same position as existing `Save Before Building?` modal) so they can reference `assetBrowserActions` which is only available after `renderAssetBrowser()` returns
- `SceneRenameState` uses static singleton — same pattern as `AssetBrowserSession`, avoids heap allocation for UI state that lives for one frame at a time

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None. All features are fully wired:
- NewScenePopup creates actual .scene files via saveLevelDef
- DeleteSceneConfirm performs filesystem remove and state cleanup
- Rename performs fs::rename and updates project.cfg + document state
- Active highlight reads live from ui.pendingScenePath each frame

## Self-Check

- [x] `src/editor/ui/LevelEditorUi.h` contains `newSceneRequested`, `deleteScenePath`, `renameScenePath`
- [x] `EditorAssetBrowserPanel.cpp` contains `SceneRenameState`, `ImVec4(0.4f, 0.8f, 0.4f, 1.0f)`, `[active]`, `+##NewScene`, `writeProjectCfgLastScene`, `document.setScenePath`
- [x] `apps/level_editor/main.cpp` contains `NewScenePopup`, `DeleteSceneConfirm`, `UnsavedChangesOnSwitch`, `File > New Scene...`, `saveLevelDef`, `hasPlayerSpawn = true`, `document.clear()`
- [x] `level-editor` builds without errors (confirmed `[100%] Built target level-editor`)

## Self-Check: PASSED
