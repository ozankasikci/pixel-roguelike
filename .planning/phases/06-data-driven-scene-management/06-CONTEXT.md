# Phase 6: Data-Driven Scene Management - Context

**Gathered:** 2026-03-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace hardcoded scene classes with fully data-driven scene loading. Add "New Scene" and "Delete Scene" to the editor UI, make the runtime default scene configurable via auto-tracking of the last-opened scene, and remove legacy scene classes (CathedralScene, SilosCloisterScene, WardenOfficeScene) in favor of GenericFileScene. Consolidate asset registration into ContentRegistry.

</domain>

<decisions>
## Implementation Decisions

### New Scene Creation
- **D-01:** New scenes contain bare minimum — player spawn at origin + default environment profile only. Clean slate.
- **D-02:** New Scene accessible from both File menu (File > New Scene) and a '+' button in the asset browser's scenes section.
- **D-03:** Creating a new scene does NOT auto-open it. The file is created in assets/scenes/ and the user opens it manually from the asset browser.
- **D-04:** New scenes always go into assets/scenes/ — flat directory, no subdirectory selection.

### Default Scene Configuration
- **D-05:** Editor auto-saves the last-opened scene path to `assets/project.cfg` every time a scene is opened. Runtime reads this as the default scene on startup. No manual override.
- **D-06:** `assets/project.cfg` is a simple key=value text file (e.g. `last_scene=warden_office.scene`).
- **D-07:** If project.cfg doesn't exist or has no last_scene entry (first launch), show a simple ImGui scene picker — list of available .scene files from assets/scenes/ with a Launch button.
- **D-08:** Editor also reads project.cfg on startup and auto-loads the last-opened scene.

### Scene Browser UX
- **D-09:** Asset browser gets a "Scenes" category listing all .scene files from assets/scenes/. Double-click to open.
- **D-10:** Right-click context menu on scenes: Delete (with confirmation dialog), Rename.
- **D-11:** Deleting the currently open scene closes it and shows an empty editor state with a prompt to create or open a scene.
- **D-12:** The currently open scene is visually marked in the asset browser (active badge/highlight).
- **D-13:** Switching scenes with unsaved changes prompts "Save changes to X before opening Y?" with Save / Don't Save / Cancel.
- **D-14:** Rename support: right-click > Rename renames the .scene file on disk and updates project.cfg if it was the last-opened scene.
- **D-15:** Duplicate Scene deferred — not this phase.

### Asset Registration
- **D-16:** Register all assets unconditionally — GenericFileScene already does this. No per-scene asset declarations.
- **D-17:** Consolidate registerCathedralAssets() and registerPrisonAssets() into a single ContentRegistry::registerAll(). Inline both functions' registrations into the consolidated method.
- **D-18:** Delete legacy scene class files entirely (CathedralScene.h/cpp, SilosCloisterScene.h/cpp, WardenOfficeScene.h/cpp). Git history preserves them.
- **D-19:** Delete the individual asset registration functions after inlining into ContentRegistry::registerAll().

### Claude's Discretion
- project.cfg parsing implementation (simple line-based parser vs reusing any existing config parsing)
- Empty editor state UI when no scene is loaded
- Asset browser scenes tab implementation details (refresh strategy, sorting)
- How the scene picker dialog integrates into the runtime main loop before gameplay starts
- CMake changes needed when removing legacy scene files

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scene system (code being refactored)
- `src/game/scenes/GenericFileScene.h` — The replacement for all legacy scenes; loads any .scene file by path
- `src/game/scenes/GenericFileScene.cpp` — Implementation: registers both asset sets, loads via LevelLoader
- `src/game/scenes/CathedralScene.h` — Legacy scene to be removed
- `src/game/scenes/CathedralScene.cpp` — Legacy scene to be removed
- `src/game/scenes/SilosCloisterScene.h` — Legacy scene to be removed
- `src/game/scenes/SilosCloisterScene.cpp` — Legacy scene to be removed
- `src/game/scenes/WardenOfficeScene.h` — Legacy scene to be removed
- `src/game/scenes/WardenOfficeScene.cpp` — Legacy scene to be removed

### Runtime scene loading
- `apps/runtime/main.cpp` — Runtime entry point; lines 72-80 have hardcoded WardenOfficeScene default
- `src/engine/scene/SceneManager.h` — Stack-based scene management
- `src/engine/scene/SceneManager.cpp` — push/pop/activeScene implementation

### Level data pipeline
- `src/game/level/LevelDef.h` — LevelDef struct, loadLevelDef(), saveLevelDef()
- `src/game/level/LevelLoader.h` — LevelLoadRequest and load() for entity spawning

### Asset registration (to be consolidated)
- `src/game/content/ContentRegistry.h` — Content resolution system; target for registerAll()
- Asset registration functions (find via grep for registerCathedralAssets, registerPrisonAssets)

### Editor scene management
- `src/editor/scene/EditorSceneDocument.h` — Scene document with load/save, dirty tracking
- `src/editor/core/LevelEditorCore.h` — loadSceneIntoEditor(), editor scene workflow
- `src/editor/core/LevelEditorCore.cpp` — Implementation of scene loading flow
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — Asset browser panel; target for scenes tab

### Scene files
- `assets/scenes/cathedral.scene` — Example .scene file format
- `assets/scenes/silos_cloister.scene` — Example .scene file
- `assets/scenes/warden_office.scene` — Example .scene file

### Project context
- `.planning/PROJECT.md` — Core value, constraints
- `.planning/ROADMAP.md` — Phase 6 goal and dependencies

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **GenericFileScene**: Already implements data-driven scene loading for any .scene path. This becomes the sole scene class.
- **EditorSceneDocument**: Full scene document model with load/save, dirty tracking, undo/redo state capture. Scene switching logic builds on this.
- **EditorAssetBrowserPanel**: Existing asset browser UI. Adding a "Scenes" tab follows the same pattern as existing asset categories.
- **LevelEditorCore::loadSceneIntoEditor()**: Existing scene loading flow with command stack reset, selection reset, preview rebuild. Extend for scene switching with unsaved-changes prompts.

### Established Patterns
- **Scene file format**: Text-based, line-per-entity format. New scenes write a minimal version of this (player_spawn line + environment_profile line).
- **Asset browser pattern**: EditorAssetBrowserPanel already lists meshes/materials. Scenes tab follows the same listing + interaction pattern.
- **ImGui modal dialogs**: Used in editor for build confirmation, unsaved changes. Reuse for delete confirmation and save-before-switch prompts.

### Integration Points
- **Runtime main.cpp**: Replace WardenOfficeScene instantiation with project.cfg reading + GenericFileScene
- **Editor main.cpp / LevelEditorCore**: Add scene auto-load from project.cfg, write-back on scene open
- **ContentRegistry**: Target for consolidated registerAll()
- **CMakeLists.txt**: Remove legacy scene .cpp files from game target sources

</code_context>

<specifics>
## Specific Ideas

- The runtime scene picker on first launch is a simple ImGui list — not a full scene browser. Just enough to select and launch.
- project.cfg should be committed to version control so team members share the same default scene.
- The empty editor state (after deleting current scene) should clearly prompt the user to create or open a scene — not just show a blank viewport.
- Scene rename must update project.cfg if the renamed scene was the last_scene value.

</specifics>

<deferred>
## Deferred Ideas

- Duplicate Scene (right-click > Duplicate with _copy suffix) — easy to add later
- Per-scene asset declarations in .scene file format — not needed at current scale
- Scene thumbnails/previews in asset browser — visual but significant work
- Scene templates (Empty, Basic Room, Corridor) for New Scene — start simple with bare minimum
- Subdirectory organization for scenes — flat assets/scenes/ is adequate now

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-data-driven-scene-management*
*Context gathered: 2026-03-30*
