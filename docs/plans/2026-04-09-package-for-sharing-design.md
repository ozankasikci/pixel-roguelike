# Package for Sharing - Design

## Goal

Consolidate "Build Settings" and "Package for Sharing" into a single "Package for Sharing..." menu item that opens a popup, builds Release, and produces a `.app` bundle.

## Menu Changes

- Remove: "Build Settings..." menu item and its popup
- Remove: "Package for Sharing" menu item
- Add: "Package for Sharing..." menu item (opens combined popup)
- Keep: Build, Build and Run, Open Build Folder, Configuration submenu

## Popup Layout

- Header: "Package for Sharing"
- Target platform: "macOS" label (only option for now)
- Scene list: checkboxes for each `.scene` file. User must explicitly select. Selection persists via `editor_build.ini`.
- Footer: "Package" button (disabled when no scenes selected) + "Cancel" button

## Package Flow

1. User clicks "Package for Sharing..." in Build menu
2. Popup opens with persisted scene selection
3. User selects scenes, clicks "Package"
4. Save scene if dirty
5. Force build config to Release, trigger build
6. On build success: create `.app` bundle in `build-package/`
7. Open `build-package/` in Finder
8. Restore previous build config

## `.app` Bundle Structure

```
build-package/
  PixelRoguelike.app/
    Contents/
      Info.plist
      MacOS/
        pixel-roguelike
      Resources/
        assets/
          shaders/
          defs/
          prefabs/
          materials/
          fonts/
          environments/
          scenes/        (selected only)
          meshes/        (referenced only)
          skies/         (referenced only)
          textures/      (referenced only)
        project.cfg      (if exists)
```

## Files to Change

1. `apps/level_editor/main.cpp` - Replace menu items and popups
2. `src/editor/build/EditorBuildSystem.cpp` - Modify `packageGame()` to output `.app` bundle with `Info.plist`
3. `src/editor/build/EditorBuildSystem.h` - No struct changes; empty `buildScenes` now means "none selected"

## Notes

- PathUtils Strategy 3 already resolves `Contents/Resources/assets/` from `.app` bundles
- Asset collection logic (scanning materials for textures, scenes for meshes) stays unchanged
- `Info.plist` generated with `CFBundleExecutable: pixel-roguelike`, `NSHighResolutionCapable: true`
