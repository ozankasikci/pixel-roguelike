---
phase: quick
plan: 260406-2yj
subsystem: editor/build-ui
tags: [editor, imgui, build-settings, ux]
key-files:
  modified:
    - apps/level_editor/main.cpp
decisions:
  - Re-added full Build Settings tabbed modal (re-implementing content removed in 3b7addf rollback) with corrected 400x350 initial size
  - Placed openFolderInOS() in the anonymous namespace at top of main.cpp, following same pattern as revealInFinder() in EditorAssetBrowserPanel.cpp
metrics:
  duration: ~5 minutes
  completed: 2026-04-05T23:17:31Z
  tasks_completed: 1
  files_modified: 1
---

# Quick Task 260406-2yj: Fix Build Settings Window Shrinking and Add Open Build Folder Summary

**One-liner:** Re-add Build Settings tabbed modal with stable 400x350 initial size and restore Open Build Folder functionality via cross-platform openFolderInOS() helper.

## Tasks Completed

| Task | Description | Commit |
| ---- | ----------- | ------ |
| 1 | Fix Build Settings window shrinking and add Open Build Folder | 3b7c292 |

## What Was Done

The 3b7addf rollback commit had removed the Build Settings modal and related variables from main.cpp. This task re-adds them with the corrected window sizing and adds the Open Build Folder convenience features.

**Changes to apps/level_editor/main.cpp:**

1. Added `#include <cstdlib>` for `std::system()` usage
2. Added `openFolderInOS()` cross-platform helper in the anonymous namespace — uses `open` on macOS, `explorer` on Windows, `xdg-open` on Linux
3. Re-added `buildSettingsPopupRequested` bool variable
4. Re-added `allBuildableScenes = listBuildableScenes()` initialization
5. Added "Build Settings..." menu item in Build menu (opens tabbed modal)
6. Added "Open Build Folder" menu item in Build menu (opens buildConfig.buildDir in OS file manager)
7. Re-added Build Settings tabbed modal with `ImVec2(400.0f, 350.0f)` initial size (was `0.0f` height, causing shrink-on-reopen)
8. Added "Open Folder" SmallButton next to "Build Succeeded" text in Build Output panel

## Deviations from Plan

**1. [Rule 1 - Bug] Build Settings modal was fully absent, not just mis-sized**

- **Found during:** Task 1
- **Issue:** The plan described fixing `ImVec2(400.0f, 0.0f)` at line 978, but commit 3b7addf had completely removed the Build Settings modal. The fix required re-adding the entire modal, not just changing a size value.
- **Fix:** Reconstructed the full tabbed Build Settings modal from commit 0155f47 content, with the corrected `ImVec2(400.0f, 350.0f)` initial size.
- **Files modified:** apps/level_editor/main.cpp
- **Commit:** 3b7c292

## Self-Check: PASSED

- `apps/level_editor/main.cpp` — modified and committed at 3b7c292
- Commit 3b7c292 exists in git log
- Build: `cmake --build build --target level-editor` succeeded
- `grep "350.0f"` finds `ImVec2(400.0f, 350.0f)` at line 995
- `grep "Open Build Folder"` finds menu item at line 787
- `grep "Open Folder"` finds SmallButton at line 2281
