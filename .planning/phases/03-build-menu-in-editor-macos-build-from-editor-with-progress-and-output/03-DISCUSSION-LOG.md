# Phase 3: Build Menu in Editor - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-29
**Phase:** 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output
**Areas discussed:** Build scope and targets, Build output and progress, Build lifecycle and controls, Run after build

---

## Build Scope and Targets

| Option | Description | Selected |
|--------|-------------|----------|
| Game only | Just "Build Game" (pixel-roguelike target). Editor and viewer are developer tools built from CLI. | ✓ |
| All three targets | Game, Editor, and Model Viewer as separate menu items. | |
| Game + Build All | "Build Game" plus a "Build All" that builds everything. | |

**User's choice:** Game only
**Notes:** Editor and model viewer remain CLI-only build targets.

| Option | Description | Selected |
|--------|-------------|----------|
| Fixed "build" directory | Always use ./build. Matches existing Makefile convention. | |
| Configurable via settings | Let user set a custom build directory in editor preferences. | ✓ |

**User's choice:** Configurable via settings
**Notes:** Stored in editor preferences file (persistent JSON/INI next to editor_window.ini).

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-configure if needed | Check if CMakeCache.txt exists. If not, run configure first. | ✓ |
| Separate Configure menu item | Explicit "Configure" and "Build" as separate actions. | |
| Always configure before build | Run cmake configure every time. | |

**User's choice:** Auto-configure if needed

| Option | Description | Selected |
|--------|-------------|----------|
| Show and allow switching | Display current config (Debug/Release/RelWithDebInfo) and let user switch. | ✓ |
| Fixed to current config | Just use whatever CMake was configured with. | |
| You decide | Claude picks. | |

**User's choice:** Show and allow switching
**Notes:** Switching config triggers automatic reconfigure.

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-reconfigure on switch | Switching config runs cmake configure automatically. | ✓ |
| Just update the setting | Store new config choice but don't reconfigure until next build. | |

**User's choice:** Auto-reconfigure on switch

| Option | Description | Selected |
|--------|-------------|----------|
| Assume working directory | CMakeLists.txt at root where editor was launched. | ✓ |
| Configurable project root | Let user set project root path in preferences. | |

**User's choice:** Assume working directory

| Option | Description | Selected |
|--------|-------------|----------|
| Unix Makefiles | Default CMake generator on macOS. Simple, matches current workflow. | ✓ |
| Ninja | Faster incremental builds. Requires ninja installed. | |
| Configurable | Let user pick generator in preferences. | |

**User's choice:** Unix Makefiles

| Option | Description | Selected |
|--------|-------------|----------|
| Raw executable for now | Just the executable binary. App bundle is separate concern. | ✓ |
| App bundle (.app) | Proper macOS .app bundle with Info.plist. | |

**User's choice:** Raw executable for now

---

## Build Output and Progress

| Option | Description | Selected |
|--------|-------------|----------|
| Dockable output panel | New "Build Output" panel. Can be docked, moved, hidden. Scrollable log. | ✓ |
| Modal dialog | Popup window that blocks editor during build. | |
| Status bar only | Minimal progress indicator. No detailed log. | |

**User's choice:** Dockable output panel

| Option | Description | Selected |
|--------|-------------|----------|
| Raw output with color hints | Raw cmake/compiler output. Errors in red, warnings in yellow. Monospace. | ✓ |
| Parsed structured output | Parse into table (file, line, message, severity). | |
| Raw output only | Plain text dump. No formatting. | |

**User's choice:** Raw output with color hints

| Option | Description | Selected |
|--------|-------------|----------|
| CMake percentage in menu bar | Parse cmake's [XX%] and show in menu bar. Lightweight. | ✓ |
| Progress bar in output panel | Full progress bar widget at top of output panel. | |
| Both | Menu bar percentage + output panel progress bar. | |

**User's choice:** CMake percentage in menu bar

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-show on build start | Panel automatically appears when Build triggered. | ✓ |
| Always visible | Panel always shown as part of default layout. | |
| Manual only | User must open from Window > Panels menu. | |

**User's choice:** Auto-show on build start

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-scroll + jump to first error | Follow output. On failure, scroll to first error. | ✓ |
| Auto-scroll only | Always follow latest output. | |
| No auto-scroll | User scrolls manually. | |

**User's choice:** Auto-scroll + jump to first error on failure

| Option | Description | Selected |
|--------|-------------|----------|
| Clear on new build | Each build starts fresh. | ✓ |
| Accumulate with separator | Timestamp separator between builds. | |

**User's choice:** Clear on new build

---

## Build Lifecycle and Controls

| Option | Description | Selected |
|--------|-------------|----------|
| Cancel button in output panel | "Stop Build" button. Sends SIGTERM to cmake process. | ✓ |
| Cancel from menu only | Build > Cancel Build menu item. | |
| No cancel | Build runs to completion. | |

**User's choice:** Cancel button in output panel

| Option | Description | Selected |
|--------|-------------|----------|
| Prompt if unsaved | Ask "Save before building?" with Save/Don't Save/Cancel. | ✓ |
| Always auto-save | Silently save before every build. | |
| Never auto-save | Build regardless of unsaved state. | |

**User's choice:** Prompt if unsaved

| Option | Description | Selected |
|--------|-------------|----------|
| Disable Build while running | Grey out "Build Game" during build. No concurrent builds. | ✓ |
| Allow re-trigger | Clicking Build cancels current and starts new. | |

**User's choice:** Disable Build while running

---

## Run After Build

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, as a menu item | Build > Build and Run. Builds then launches on success. | ✓ |
| Yes, with toolbar play button | Menu item plus toolbar Play/Build button. | |
| No, just Build | Only build. User launches manually. | |

**User's choice:** Yes, as a menu item

| Option | Description | Selected |
|--------|-------------|----------|
| Pass current scene as argument | Launch with --scene <current_scene_path>. | ✓ |
| Game's default scene | Launch game normally with default scene. | |
| You decide | Claude picks based on game executable support. | |

**User's choice:** Pass current scene as argument

| Option | Description | Selected |
|--------|-------------|----------|
| Show error, don't run | Stay in editor, show errors. No attempt to run. | ✓ |
| Offer to run last successful build | Ask to run previous binary if exists. | |

**User's choice:** Show error, don't run

---

## Claude's Discretion

- Process spawning mechanism
- Output panel styling and exact layout
- Keyboard shortcut assignments (Cmd+B, Cmd+R)
- CMake percentage parsing implementation
- Thread/async model for non-blocking builds
- Editor preferences file format details

## Deferred Ideas

- Windows build support — future phase
- App bundle (.app) packaging
- Ninja generator support
- Editor self-rebuild
- Model viewer build from editor
- "Build All" option
- Toolbar Play/Build button
- "Run last successful build" fallback
