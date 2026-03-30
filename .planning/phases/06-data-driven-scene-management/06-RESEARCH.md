# Phase 6: Data-Driven Scene Management - Research

**Researched:** 2026-03-30
**Domain:** Editor UX, runtime scene loading, C++ file I/O, ImGui modal workflows
**Confidence:** HIGH — entire implementation is in-repo code; no external library choices involved

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** New scenes contain bare minimum — player spawn at origin + default environment profile only. Clean slate.
- **D-02:** New Scene accessible from both File menu (File > New Scene) and a '+' button in the asset browser's scenes section.
- **D-03:** Creating a new scene does NOT auto-open it. The file is created in assets/scenes/ and the user opens it manually.
- **D-04:** New scenes always go into assets/scenes/ — flat directory, no subdirectory selection.
- **D-05:** Editor auto-saves the last-opened scene path to `assets/project.cfg` every time a scene is opened. Runtime reads this as the default scene on startup.
- **D-06:** `assets/project.cfg` is a simple key=value text file (e.g. `last_scene=warden_office.scene`).
- **D-07:** If project.cfg doesn't exist or has no last_scene entry (first launch), show a simple ImGui scene picker — list of available .scene files from assets/scenes/ with a Launch button.
- **D-08:** Editor also reads project.cfg on startup and auto-loads the last-opened scene.
- **D-09:** Asset browser gets a "Scenes" category listing all .scene files from assets/scenes/. Double-click to open.
- **D-10:** Right-click context menu on scenes: Delete (with confirmation dialog), Rename.
- **D-11:** Deleting the currently open scene closes it and shows an empty editor state with a prompt to create or open a scene.
- **D-12:** The currently open scene is visually marked in the asset browser (active badge/highlight).
- **D-13:** Switching scenes with unsaved changes prompts "Save changes to X before opening Y?" with Save / Don't Save / Cancel.
- **D-14:** Rename support: right-click > Rename renames the .scene file on disk and updates project.cfg if it was the last-opened scene.
- **D-15:** Duplicate Scene deferred — not this phase.
- **D-16:** Register all assets unconditionally. No per-scene asset declarations.
- **D-17:** Consolidate registerCathedralAssets() and registerPrisonAssets() into a single ContentRegistry::registerAll(). Inline both functions' registrations into the consolidated method.
- **D-18:** Delete legacy scene class files entirely (CathedralScene.h/cpp, SilosCloisterScene.h/cpp, WardenOfficeScene.h/cpp). Git history preserves them.
- **D-19:** Delete the individual asset registration functions after inlining into ContentRegistry::registerAll().

### Claude's Discretion

- project.cfg parsing implementation (simple line-based parser vs reusing any existing config parsing)
- Empty editor state UI when no scene is loaded
- Asset browser scenes tab implementation details (refresh strategy, sorting)
- How the scene picker dialog integrates into the runtime main loop before gameplay starts
- CMake changes needed when removing legacy scene files

### Deferred Ideas (OUT OF SCOPE)

- Duplicate Scene (right-click > Duplicate with _copy suffix)
- Per-scene asset declarations in .scene file format
- Scene thumbnails/previews in asset browser
- Scene templates (Empty, Basic Room, Corridor) for New Scene
- Subdirectory organization for scenes
</user_constraints>

---

## Summary

Phase 6 is a surgical refactoring and feature addition phase within a well-understood, single-repo C++ codebase. There are no external library choices — all work uses existing project patterns (ImGui, std::filesystem, fstream). The phase has five logical areas: (1) consolidate asset registration into ContentRegistry::registerAll() on MeshLibrary, (2) delete three legacy scene classes and update CMake, (3) write a project.cfg reader/writer and wire it into both the editor and runtime startup, (4) add New Scene / Delete Scene / Rename Scene operations to the editor, and (5) update the asset browser to show a Scenes category with the active scene marked.

The most structurally careful work is the runtime scene picker (D-07): it must run before `app.run()` but after the Application/systems are initialized. The cleanest approach is a brief pre-loop that shows an ImGui fullscreen window over the GLFW window until the user selects a scene and clicks Launch. This keeps the runtime main.cpp linear and does not require threading or a new Scene subclass for the picker state itself.

The registerAll() consolidation (D-17) is about moving mesh registration, not ContentRegistry's JSON-file loading. `registerCathedralAssets(MeshLibrary&)` and `registerPrisonAssets(MeshLibrary&)` both create procedural meshes and call `meshLibrary.registerMesh()`. The target is a new `ContentRegistry::registerAll(MeshLibrary&)` static/free function (or a standalone `registerAllGameAssets(MeshLibrary&)`) that inlines both call sites. All four call sites (GenericFileScene, EditorPreviewWorld, RuntimeGameSession::bootstrapRuntimeMeshLibrary, and any EditorRuntimePreviewSession) must be updated.

**Primary recommendation:** Implement in this order — (1) registerAll consolidation, (2) legacy scene deletion + CMake, (3) project.cfg I/O utilities, (4) runtime startup changes, (5) editor startup changes, (6) New Scene creation, (7) Delete/Rename operations with modal dialogs, (8) asset browser Scenes category with active highlight.

---

## Standard Stack

### Core (all already in project — no new dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Dear ImGui | v1.92.6 | Modal dialogs, inline rename, scene picker overlay | Already integrated via GLFW+OpenGL3 backend |
| std::filesystem | C++20 STL | File creation, rename, delete, directory scan | Zero-dep, already used throughout codebase |
| std::fstream / std::ifstream / std::ofstream | C++20 STL | project.cfg line-based read/write | Same pattern as existing `loadWindowGeometry()` in editor main |
| GLFW | 3.4 | Render loop for runtime scene picker overlay | Already the windowing layer |

### No new installations needed

All required functionality is already in the project. This phase is purely in-repo code work.

---

## Architecture Patterns

### project.cfg Format and I/O

**Decision D-06:** Simple key=value text file, one key per line.

```
last_scene=warden_office.scene
```

The value stores only the filename (stem + extension), not the full path. The reader prepends `assets/scenes/` when constructing the full path. This follows the same pattern as `editor_window.ini` (the existing `loadWindowGeometry()` / `saveWindowGeometry()` functions in `apps/level_editor/main.cpp`).

**Recommended project.cfg utilities (new file: `src/engine/core/ProjectConfig.h/.cpp` or inline in a game-layer header):**

```cpp
// Source: modeled on loadWindowGeometry() in apps/level_editor/main.cpp
std::string readProjectCfgLastScene(const std::string& cfgPath);
void writeProjectCfgLastScene(const std::string& cfgPath, const std::string& sceneFilename);
```

The path for project.cfg is `assets/project.cfg`, resolved via `resolveProjectPath("assets/project.cfg")`.

**Reading:**
```cpp
std::string readProjectCfgLastScene(const std::string& cfgPath) {
    std::ifstream file(cfgPath);
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("last_scene=", 0) == 0) {
            return line.substr(11); // "last_scene=".length()
        }
    }
    return {};
}
```

**Writing** (write-back on every scene open in the editor): rewrite the whole file since it is tiny.

### registerAll Consolidation (D-17)

The four call sites that currently call both `registerCathedralAssets` and `registerPrisonAssets` are:

| File | Function |
|------|----------|
| `src/game/scenes/GenericFileScene.cpp` | `GenericFileScene::onEnter()` lambda |
| `src/editor/scene/EditorPreviewWorld.cpp` | `EditorPreviewWorld()` constructor |
| `src/game/runtime/RuntimeGameSession.cpp` | `bootstrapRuntimeMeshLibrary()` |
| Check for EditorRuntimePreviewSession too | (grep needed at plan time) |

**Target signature — new free function in `game_content` lib:**

```cpp
// src/game/levels/GameAssets.h
#pragma once
#include "engine/rendering/geometry/MeshLibrary.h"
void registerAllGameAssets(MeshLibrary& meshLibrary);
```

The function body is the union of `registerCathedralAssets` + `registerPrisonAssets` with duplicate `registerDefaults()` calls collapsed to one. Both source .cpp files are deleted after inlining.

**CMake impact:** Remove `levels/cathedral/CathedralAssets.cpp` and `levels/prison/PrisonAssets.cpp` from `game_content`. Add `levels/GameAssets.cpp`. Remove header files too.

**Alternative:** Add `registerAll(MeshLibrary&)` as a static method on ContentRegistry (D-17 says "ContentRegistry::registerAll()"). This works but requires ContentRegistry.h to include MeshLibrary.h, which is a new dependency. A free function in a new `GameAssets.h` keeps ContentRegistry's header clean. Either approach is valid — the planner should use a free function unless the team specifically wants it on ContentRegistry.

### Legacy Scene Deletion (D-18)

Files to delete:
- `src/game/scenes/CathedralScene.h` / `.cpp`
- `src/game/scenes/SilosCloisterScene.h` / `.cpp`
- `src/game/scenes/WardenOfficeScene.h` / `.cpp`

**CMake:** In `src/game/CMakeLists.txt`, remove from `gameplay` target sources:
```
scenes/CathedralScene.cpp
scenes/SilosCloisterScene.cpp
scenes/WardenOfficeScene.cpp
```

**Runtime main.cpp cleanup:**
- Remove `#include "game/scenes/SilosCloisterScene.h"` and `#include "game/scenes/WardenOfficeScene.h"`
- Replace the hardcoded `WardenOfficeScene` fallback with project.cfg reading logic

### Runtime Startup (D-05, D-07, D-08)

**Current flow** (`apps/runtime/main.cpp` lines 72-80):
```cpp
if (scenePath.empty()) {
    sceneManager.pushScene(std::make_unique<WardenOfficeScene>(), app);
} else {
    sceneManager.pushScene(std::make_unique<GenericFileScene>(scenePath), app);
}
```

**New flow:**
```cpp
// 1. --scene arg takes priority (existing)
// 2. project.cfg last_scene key
// 3. First-launch picker (no project.cfg / empty last_scene)

if (scenePath.empty()) {
    const std::string cfgScene = readProjectCfgLastScene(resolveProjectPath("assets/project.cfg"));
    if (!cfgScene.empty()) {
        scenePath = resolveProjectPath("assets/scenes/" + cfgScene);
    }
}

if (scenePath.empty()) {
    // show ImGui scene picker until user picks a scene
    scenePath = runRuntimeScenePicker(app, window);
}

sceneManager.pushScene(std::make_unique<GenericFileScene>(scenePath), app);
```

**Runtime scene picker implementation:** A synchronous pre-loop inside `runRuntimeScenePicker()` that:
1. Calls `sortedScenePaths()` (already exists in `LevelEditorCore.cpp`) to list available scenes
2. Renders a fullscreen ImGui window with a listbox and a "Launch" button
3. Runs its own minimal `glfwPollEvents()` / `ImGui::NewFrame()` / render / `glfwSwapBuffers()` loop
4. Returns when the user clicks Launch

This pattern is identical to the editor's `renderStartupProgress()` helper — a blocking pre-loop before `app.run()`.

### Editor Startup (D-08)

Current hardcoded line 253 in `apps/level_editor/main.cpp`:
```cpp
const std::string initialScene = argc > 1 ? argv[1] : "assets/scenes/silos_cloister.scene";
```

New logic:
```cpp
std::string initialScene = argc > 1 ? argv[1] : "";
if (initialScene.empty()) {
    const std::string cfgScene = readProjectCfgLastScene(resolveProjectPath("assets/project.cfg"));
    if (!cfgScene.empty()) {
        initialScene = resolveProjectPath("assets/scenes/" + cfgScene);
    }
}
// If still empty, editor shows empty state (no scene loaded)
```

When `initialScene` is empty, the editor must handle a `document` with no scene loaded. This affects the startup sequence — `document.loadFromSceneFile()` is only called when `initialScene` is non-empty.

### Empty Editor State (Claude's Discretion)

When no scene is loaded (first launch or after deleting the active scene), the editor viewport should show a centered ImGui overlay:

```
No scene loaded
[New Scene]  [Open Scene...]
```

"Open Scene" triggers the same asset browser double-click flow. "New Scene" creates a blank scene. This overlay renders in place of the normal viewport content.

`EditorUiState::pendingScenePath` being empty is the signal for this state. Guard `previewWorld.rebuild()`, `loadSceneIntoEditor()`, and the render path with `!ui.pendingScenePath.empty()`.

### New Scene Creation (D-01 to D-04)

**Minimal .scene file content** (based on `serializeLevelDef()` output format):
```
environment_profile default

player_spawn 0.0 0.0 0.0 -8.0 node node_1
```

The `environment_profile` line uses `"default"` which resolves to the neutral environment. The `player_spawn` line places the player at the origin. This is all `serializeLevelDef()` outputs for a `LevelDef` with only `hasPlayerSpawn=true` and default `environmentId`.

**New scene creation function:**
```cpp
// Returns the path of the created file, or empty string on failure
std::string createNewScene(const std::string& name);
```

1. Validate that `name` is a valid filename (no path separators, no `.scene` double-extension)
2. Build path: `resolveProjectPath("assets/scenes/" + name + ".scene")`
3. Check for collision: if file already exists, append `_1`, `_2`, etc.
4. Build a minimal `LevelDef` with `hasPlayerSpawn = true` and default fields
5. Call `saveLevelDef(path, def)`
6. Invalidate asset browser cache (call `refreshAssetTree = true`)

**Name input UI:** A small ImGui InputText popup triggered by File > New Scene or the '+' button. Validates on Enter or OK button press.

### Delete Scene (D-10, D-11)

```cpp
// Modal popup ID: "DeleteSceneConfirm"
bool deleteScene(const std::string& scenePath);
```

1. Show ImGui modal: "Delete 'warden_office.scene'? This cannot be undone."  [Delete] [Cancel]
2. On confirm: `std::filesystem::remove(absolutePath)`
3. If deleted scene == `ui.pendingScenePath`: clear document, set `ui.pendingScenePath = ""`
4. If deleted scene == project.cfg `last_scene`: clear project.cfg entry
5. Refresh asset browser cache

**ImGui modal pattern** (existing in codebase for build confirmation):
```cpp
if (ImGui::BeginPopupModal("DeleteSceneConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Delete '%s'? This cannot be undone.", sceneName.c_str());
    if (ImGui::Button("Delete")) {
        // perform deletion
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}
```

### Rename Scene (D-14)

ImGui inline rename: a selectable row that switches to an `InputText` when Rename is triggered from context menu.

```cpp
// 1. std::filesystem::rename(oldPath, newPath)
// 2. If oldFilename == project.cfg last_scene: writeProjectCfgLastScene(cfg, newFilename)
// 3. If renamed scene == ui.pendingScenePath: update ui.pendingScenePath
// 4. Refresh asset browser cache
```

The inline rename pattern: store a `renamingScenePath` string in the asset browser panel. When it matches the current row, render `ImGui::InputText` instead of `ImGui::Selectable`. Commit on Enter or focus-loss.

### Unsaved Changes Guard (D-13)

The existing `loadSceneIntoEditor()` call site in the main loop (`apps/level_editor/main.cpp` line 911) is where `requestedScenePath` is processed. Wrap this with a dirty-check:

```cpp
if (requestedScenePath.has_value()) {
    if (document.dirty()) {
        // open modal "Save changes to X before opening Y?"
        pendingSceneSwitch = requestedScenePath;
        ImGui::OpenPopup("UnsavedChangesOnSwitch");
    } else {
        loadSceneIntoEditor(*requestedScenePath, ...);
        writeProjectCfgLastScene(cfgPath, filename);
    }
    requestedScenePath.reset();
}
```

The modal has three buttons: Save (save then switch), Don't Save (switch without saving), Cancel (abort).

### Asset Browser Scenes Category (D-09, D-12)

The existing `EditorAssetBrowserKind::Scene` enum value and `buildProjectAssetBrowserTree()` already classify `.scene` files with `kind = EditorAssetBrowserKind::Scene`. The double-click handler already fires `result.openScenePath`. The right-click context menu for Scene nodes currently only has "Open Scene" and "Reveal in Finder".

**Changes required:**
1. Add "New Scene..." item to the right-click context menu for Scene entries (and for the scenes folder node)
2. Add "Delete..." item (opens modal)
3. Add "Rename" item (enters inline rename mode)
4. Visual highlight for active scene: check `node.relativePath == ui.pendingScenePath` when rendering — use `ImGuiTreeNodeFlags_Selected` or a colored text indicator

The asset browser '+' button for new scenes: add a small `[+]` `ImGui::Button` adjacent to the "Scenes" folder row, or in the top bar with a filter.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Config file format | Custom binary or JSON parser | Line-based `key=value` read with `std::getline` | project.cfg has exactly one key right now; `loadWindowGeometry()` in the same codebase is the proven pattern |
| File rename atomicity | Manual copy+delete | `std::filesystem::rename()` | On same filesystem this is atomic on POSIX; handles name collision naturally |
| Scene file validation | Custom parser | `loadLevelDef()` already validates | The save path for new scenes uses `saveLevelDef()` which goes through the serializer; round-trip is already tested by the existing scenes |
| Modal dialog system | Custom popup class | `ImGui::OpenPopup()` + `ImGui::BeginPopupModal()` | Already used elsewhere in editor (build confirmation modal) |
| Directory watching / auto-refresh | `inotify`/`FSEvents` watcher | Explicit `refreshAssetTree = true` flag on write operations | Overkill; the asset browser already has F5 + manual Refresh; deterministic refresh on mutating operations is sufficient |

---

## Common Pitfalls

### Pitfall 1: project.cfg path resolution varies by working directory

**What goes wrong:** Both the editor and runtime use `resolveProjectPath()` to locate assets relative to the project root. If project.cfg is written with a relative path from one working directory and read from another, the file won't be found.

**Why it happens:** `resolveProjectPath()` searches upward from the binary's location for the `assets/` directory. Both binaries are run from the build directory, not the repo root.

**How to avoid:** Always use `resolveProjectPath("assets/project.cfg")` for both reads and writes. Never hardcode `"assets/project.cfg"` as a relative path — it is relative to the binary's CWD which is the build directory, not the repo root.

**Warning signs:** project.cfg gets created in the build directory instead of `assets/`. Check that `resolveProjectPath` returns the repo-relative path.

### Pitfall 2: registerAll introduces duplicate mesh registrations

**What goes wrong:** Both `registerCathedralAssets` and `registerPrisonAssets` call `meshLibrary.registerDefaults()` at the top. If `registerAllGameAssets` naively concatenates both bodies, `registerDefaults()` is called twice.

**Why it happens:** Each function was written to be self-contained.

**How to avoid:** Call `registerDefaults()` exactly once at the top of `registerAllGameAssets`, then inline the mesh registrations from both functions.

**Warning signs:** MeshLibrary log warnings about duplicate registration, or duplicate meshes appearing in the editor's mesh list.

### Pitfall 3: Empty document state causes crashes in startup sequence

**What goes wrong:** If `initialScene` is empty on editor startup, calls like `document.loadFromSceneFile("", content)`, `previewWorld.rebuild(document, content)`, and `syncEditorCameraToRuntimeStart(document, editCamera)` are all called on an empty document. `loadFromSceneFile("")` will throw or produce garbage.

**Why it happens:** The startup sequence was written assuming a valid scene path is always available.

**How to avoid:** Gate all startup sequence operations on `!initialScene.empty()`. When `initialScene` is empty, initialize `document` with `document.clear()`, skip the `previewWorld.rebuild()` call, and show the empty editor state UI immediately.

**Warning signs:** Crash or exception on first launch (before any scene has been opened).

### Pitfall 4: Asset browser cache stale after scene file operations

**What goes wrong:** After creating/deleting/renaming a scene, the asset browser's `cachedNodes` (static `AssetBrowserSession`) still shows the old list because `cacheValid = true`.

**Why it happens:** The cache is invalidated only by `refreshAssetTree = true` being returned in `AssetBrowserActionResult`. New scene operations that happen outside the `renderAssetBrowser()` call (e.g., from File menu) don't automatically set this flag.

**How to avoid:** Return or propagate a `bool assetCatalogChanged` signal from any function that creates/deletes/renames scenes, and ensure it feeds back into `refreshAssetTree` in the main render loop.

**Warning signs:** New scene appears only after F5, or deleted scene persists in the browser.

### Pitfall 5: Rename modifies ui.pendingScenePath inconsistently

**What goes wrong:** After renaming the currently-open scene file on disk, `ui.pendingScenePath` still holds the old relative path. The window title, save operation, and build system all use `ui.pendingScenePath`.

**Why it happens:** `ui.pendingScenePath` is only updated by `loadSceneIntoEditor()`.

**How to avoid:** After a successful rename of the currently-open scene, explicitly update `ui.pendingScenePath` and `document.setScenePath()` to the new path. Do not call `loadSceneIntoEditor()` (that would reset the undo stack and lose unsaved changes).

**Warning signs:** Ctrl+S saves to the old filename after rename; window title shows old name.

### Pitfall 6: Runtime scene picker runs before ImGui font atlas is built

**What goes wrong:** If the runtime scene picker ImGui overlay is rendered before `ImGui::NewFrame()` has been called at least once, the font atlas may not be ready.

**Why it happens:** The runtime `main.cpp` doesn't have an `ImGuiLayer` initialized. Unlike the editor, the runtime currently has no ImGui integration.

**How to avoid:** The runtime scene picker requires an `ImGuiLayer` init/shutdown around the picker loop. Add `ImGuiLayer imgui; imgui.init(window.handle());` before the picker loop and `imgui.shutdown();` after. This is a temporary initialization solely for the picker. Alternatively, check if `RenderSystem` already initializes ImGui; if so, defer the picker until after `app.run()` starts. The cleanest approach is a standalone pre-loop in main.cpp with its own ImGui lifecycle.

---

## Code Examples

### Verified pattern: line-based key=value config (from existing codebase)

```cpp
// Source: apps/level_editor/main.cpp - loadWindowGeometry()
WindowGeometry loadWindowGeometry() {
    WindowGeometry geo;
    std::ifstream file(kWindowGeometryFile);
    if (!file.is_open()) {
        return geo;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (std::sscanf(line.c_str(), "x=%d", &geo.x) == 1) continue;
        if (std::sscanf(line.c_str(), "y=%d", &geo.y) == 1) continue;
        // ...
    }
    return geo;
}
```

For project.cfg, prefer `std::string::rfind("last_scene=", 0) == 0` over `sscanf` since the value is a string.

### Verified pattern: minimal LevelDef serialization (from existing serializeLevelDef)

```cpp
// Output for a new scene (LevelDef with hasPlayerSpawn=true, environmentId="default"):
// environment_profile default
//
// player_spawn 0.0 0.0 0.0 -8.0 node node_1
LevelDef makeNewSceneDef() {
    LevelDef def;
    def.environmentId = "default";
    def.hasPlayerSpawn = true;
    // playerSpawn.position defaults to {0,0,0}, fallRespawnY defaults to -8.0
    return def;
}
// then: saveLevelDef(path, makeNewSceneDef());
```

### Verified pattern: ImGui modal dialog (from existing build confirmation in editor)

```cpp
// Source: apps/level_editor/main.cpp build confirmation pattern
if (ImGui::BeginPopupModal("DeleteSceneConfirm", nullptr,
                            ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Delete '%s'? This cannot be undone.", sceneName.c_str());
    ImGui::Separator();
    if (ImGui::Button("Delete", ImVec2(80, 0))) {
        // perform deletion
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
```

### Verified pattern: asset browser active scene highlight

```cpp
// Source: existing EditorAssetBrowserPanel.cpp renderNode lambda
const bool isActive = (node.relativePath == ui.pendingScenePath);
if (node.kind == EditorAssetBrowserKind::Scene && isActive) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
}
if (ImGui::Selectable(node.name.c_str(), selected || isActive)) {
    setSelectedAsset(ui, node);
}
if (node.kind == EditorAssetBrowserKind::Scene && isActive) {
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("[active]");
}
```

### Verified pattern: sortedScenePaths already exists

```cpp
// Source: src/editor/core/LevelEditorCore.cpp
std::vector<std::string> sortedScenePaths() {
    namespace fs = std::filesystem;
    std::vector<std::string> results;
    const fs::path sceneDir(resolveProjectPath("assets/scenes"));
    if (!fs::exists(sceneDir)) { return results; }
    for (const auto& entry : fs::directory_iterator(sceneDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".scene") continue;
        results.push_back(fs::relative(entry.path(), fs::current_path()).generic_string());
    }
    std::sort(results.begin(), results.end());
    return results;
}
// Reuse this in both the runtime scene picker and the New Scene name-collision check.
```

---

## Runtime State Inventory

This phase does NOT involve renaming strings, databases, or stored data. The `project.cfg` file does not yet exist — it is being created by this phase. No migration needed.

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None — project.cfg is created fresh by this phase | None |
| Live service config | None | None |
| OS-registered state | None | None |
| Secrets/env vars | None | None |
| Build artifacts | Legacy scene .cpp files compiled into `gameplay` lib; `CathedralAssets.cpp`, `PrisonAssets.cpp` compiled into `game_content` | CMake source list update + file deletion; rebuild required |

---

## Environment Availability

This phase has no external tool dependencies beyond the project's own build system.

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build system | Yes | 4.1.1 | — |
| Apple Clang | C++ compiler | Yes | 17.0.0 (clang-1700.4.4.1) | — |
| std::filesystem | File operations (create/delete/rename) | Yes | C++20 (project already targets C++20) | — |

**No missing dependencies.**

---

## Open Questions

1. **EditorRuntimePreviewSession mesh registration**
   - What we know: `EditorPreviewWorld` calls `registerCathedralAssets` + `registerPrisonAssets` in its constructor. `RuntimeGameSession.cpp::bootstrapRuntimeMeshLibrary` does the same.
   - What's unclear: Does `EditorRuntimePreviewSession` have its own MeshLibrary bootstrap path that also calls both functions? (grep at plan time needed for `EditorRuntimePreviewSession.cpp`)
   - Recommendation: Planner should grep for additional call sites before writing the consolidation task.

2. **Runtime ImGui availability**
   - What we know: The runtime `main.cpp` has no `ImGuiLayer`. The `RenderSystem` may internally initialize ImGui for the game HUD.
   - What's unclear: Whether `RenderSystem::init()` calls ImGui init, and if so whether the scene picker can reuse that context.
   - Recommendation: Check `src/game/systems/RenderSystem.cpp` for ImGui init. If RenderSystem owns ImGui, the picker must integrate with the render loop rather than running as a pre-loop. The simplest path is a standalone `ImGuiLayer` in `main.cpp` for the picker only, destroyed before `app.run()`.

3. **New scene name uniqueness**
   - What we know: D-04 says flat directory, D-03 says no auto-open.
   - What's unclear: What name to use by default — "new_scene", "untitled", or a counter ("scene_1", "scene_2")?
   - Recommendation: Use `"new_scene"` as the default stem, appending `_1`, `_2`, etc. if a collision exists. Present an InputText pre-filled with the default name.

---

## Sources

### Primary (HIGH confidence)

All findings are drawn directly from the project codebase — no external sources required for this phase.

- `apps/runtime/main.cpp` — confirmed hardcoded `WardenOfficeScene` at lines 72-80; `--scene` arg already supported
- `apps/level_editor/main.cpp` — confirmed `loadWindowGeometry()` pattern (lines 96-125); `initialScene` hardcoded at line 253; `loadSceneIntoEditor()` call at line 912
- `src/game/scenes/GenericFileScene.cpp` — confirmed calls to both `registerCathedralAssets` + `registerPrisonAssets`; confirmed as the sole surviving scene class
- `src/editor/scene/EditorPreviewWorld.cpp` — confirmed constructor calls both asset registration functions
- `src/game/runtime/RuntimeGameSession.cpp` — confirmed `bootstrapRuntimeMeshLibrary()` calls both
- `src/editor/core/LevelEditorCore.cpp` — confirmed `sortedScenePaths()` already implemented
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — confirmed `EditorAssetBrowserKind::Scene` handled; double-click opens; context menu only has "Open Scene" + "Reveal in Finder"
- `src/game/level/LevelDef.cpp` — confirmed `serializeLevelDef()` output format; `environment_profile` and `player_spawn` tokens verified
- `src/game/CMakeLists.txt` — confirmed three legacy scene .cpp files listed; confirmed `game_content` contains `CathedralAssets.cpp` and `PrisonAssets.cpp`

### Secondary (MEDIUM confidence)

- ImGui `BeginPopupModal` pattern: confirmed present in editor main.cpp build confirmation flow (grep verified)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all work in existing C++ + ImGui + std::filesystem
- Architecture: HIGH — all integration points read from source; patterns confirmed from existing code
- Pitfalls: HIGH — derived from actual code structure (startup sequence, cache invalidation, path resolution)

**Research date:** 2026-03-30
**Valid until:** Stable (no external dependencies; only invalidated by major refactoring of editor or runtime architecture)

## Project Constraints (from CLAUDE.md)

- Engine: Custom C++ — no Unity/Unreal/Godot
- Graphics API: OpenGL 4.1 Core Profile (all shaders target GLSL 4.10)
- C++20 standard
- CMake 3.28+ with FetchContent
- Naming: `PascalCase` classes, `camelCase` functions, `snake_case` variables, trailing `_` for private members
- `#pragma once` (no include guards)
- Include order: standard library -> third-party -> project headers
- RAII for OpenGL resources
- No Boost — use C++20 STL
- `pixel_roguelike_add_test()` for test registration; tests are standalone executables
- Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`
- GSD workflow: use `/gsd:execute-phase` for planned phase work; no direct repo edits outside GSD workflow
