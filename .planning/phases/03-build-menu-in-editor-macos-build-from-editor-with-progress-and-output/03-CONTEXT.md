# Phase 3: Build Menu in Editor - Context

**Gathered:** 2026-03-29
**Status:** Ready for planning

<domain>
## Phase Boundary

Add a "Build" top-level menu to the level editor's ImGui menu bar. The menu allows the user to build the game executable for macOS using CMake, view build progress and output in a dockable panel, cancel builds, switch build configurations (Debug/Release/RelWithDebInfo), and launch the game after a successful build. Windows build support is explicitly deferred.

</domain>

<decisions>
## Implementation Decisions

### Build Scope and Targets
- **D-01:** Only the game target (`pixel-roguelike`) is buildable from the editor. Editor and model viewer are developer tools built from CLI only.
- **D-02:** Build directory is configurable via editor preferences file (persistent JSON/INI next to `editor_window.ini`). Default is `build`.
- **D-03:** Auto-configure: check if `{build_dir}/CMakeCache.txt` exists. If not, run `cmake -S . -B {build_dir}` before building.
- **D-04:** Build configuration (Debug/Release/RelWithDebInfo) is displayed in the Build menu and switchable. Switching config triggers an automatic reconfigure with `-DCMAKE_BUILD_TYPE=X`.
- **D-05:** Assume CMakeLists.txt is in the working directory (project root). No configurable project root.
- **D-06:** Use Unix Makefiles generator (default CMake on macOS). No Ninja/Xcode.
- **D-07:** Output is the raw executable binary (no .app bundle packaging).

### Build Output and Progress
- **D-08:** Build output shown in a new dockable "Build Output" panel (like Outliner, Inspector). Scrollable, monospace font.
- **D-09:** Raw compiler/cmake output with color hints: errors highlighted in red, warnings in yellow.
- **D-10:** CMake's `[XX%]` progress markers parsed and displayed as a percentage in the menu bar area during builds.
- **D-11:** Output panel auto-shows when a build starts. Can be manually closed.
- **D-12:** Auto-scroll to bottom during build. On build failure, jump to the first error line.
- **D-13:** Output cleared on each new build (no accumulation/history).

### Build Lifecycle and Controls
- **D-14:** Cancel button ("Stop Build") in the output panel header. Sends SIGTERM to the cmake process.
- **D-15:** If scene has unsaved changes when building, prompt "Save before building?" with Save/Don't Save/Cancel options.
- **D-16:** Build menu item greyed out while a build is in progress. No concurrent builds.

### Run After Build
- **D-17:** "Build and Run" menu item under Build menu. Builds the game, then launches the executable on success.
- **D-18:** When running from editor, pass `--scene <current_scene_path>` to the game executable so it loads the scene being edited.
- **D-19:** On build failure, show errors in output panel. No attempt to run. No "run last successful build" option.

### Claude's Discretion
- Process spawning mechanism (fork/exec, popen, std::system, platform-specific)
- Output panel styling and exact layout within the dock
- Keyboard shortcut assignments for Build (e.g., Cmd+B) and Build and Run (e.g., Cmd+R)
- How to parse CMake percentage from output stream
- Thread/async model for non-blocking build execution
- Editor preferences file format details

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

No external specs — requirements fully captured in decisions above.

### Editor Architecture
- `apps/level_editor/main.cpp` — Editor main loop, ImGui menu bar structure (lines 466-591), existing menu pattern
- `src/editor/ui/LevelEditorUi.h` — EditorUiState struct, panel visibility flags pattern
- `src/editor/core/LevelEditorCore.h` — Editor core utilities and layout functions

### Build System
- `CMakeLists.txt` — Project configuration, target names (`pixel-roguelike`)
- `Makefile` — Existing CLI build workflow (configure + build + run pattern)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EditorUiState` struct: Pattern for panel visibility booleans (e.g., `showOutliner`, `showInspector`). New `showBuildOutput` flag follows same pattern.
- `WindowGeometry` / `editor_window.ini`: Existing persistent editor settings pattern to extend for build preferences.
- Menu bar in `main.cpp:466-591`: Clear pattern for adding new top-level menus (File, Edit, Create, View, Window, Help). New "Build" menu slots in naturally.

### Established Patterns
- ImGui dockable panels: All existing panels (Outliner, Inspector, Asset Browser, Environment) are dockable ImGui windows. Build Output follows same pattern.
- Editor uses `spdlog` for logging.
- Build targets use `cmake --build {dir} --target {name} --parallel` pattern.

### Integration Points
- New "Build" menu added to `ImGui::BeginMenuBar()` block in `main.cpp` (between existing menus)
- New `EditorBuildState` or similar struct to track build process state (running, output buffer, progress percentage)
- Build output panel rendered as a new ImGui window in the main editor frame
- Editor preferences extended with build directory and config type settings

</code_context>

<specifics>
## Specific Ideas

- Menu bar percentage display during build (e.g., "Build [47%]" in the Build menu title while building)
- The `--scene` argument to the game executable will need to be implemented or verified to work
- Build and Run should only be available as a menu item, not a toolbar button (for this phase)

</specifics>

<deferred>
## Deferred Ideas

- Windows build support — future phase
- App bundle (.app) packaging for macOS distribution
- Ninja generator support
- Editor self-rebuild from within the editor
- Model viewer build target from editor
- "Build All" option
- Toolbar Play/Build button
- "Run last successful build" fallback on failure

</deferred>

---

*Phase: 03-build-menu-in-editor-macos-build-from-editor-with-progress-and-output*
*Context gathered: 2026-03-29*
