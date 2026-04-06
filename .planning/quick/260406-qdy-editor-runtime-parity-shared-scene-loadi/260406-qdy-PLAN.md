---
phase: quick-260406-qdy
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - src/game/scenes/GenericFileScene.h
  - src/game/scenes/GenericFileScene.cpp
  - src/editor/core/EditorRuntimePreviewSession.cpp
  - apps/level_editor/main.cpp
autonomous: true
requirements: [editor-runtime-parity]

must_haves:
  truths:
    - "Editor preview session shows scripted geometry (doors) identical to the runtime game"
    - "Stopping and restarting preview resets scripted geometry to code-defined state"
    - "Editor outliner is NOT affected — only preview session runs scripted geometry"
  artifacts:
    - path: "src/game/scenes/GenericFileScene.h"
      provides: "Public static lookup method for scripted geometry callbacks"
      contains: "lookupScriptedGeometry"
    - path: "src/game/scenes/GenericFileScene.cpp"
      provides: "Implementation of the lookup method"
      contains: "lookupScriptedGeometry"
    - path: "src/editor/core/EditorRuntimePreviewSession.cpp"
      provides: "rebuild() that populates LevelLoadRequest.buildScriptedGeometry from registry"
      contains: "lookupScriptedGeometry"
    - path: "apps/level_editor/main.cpp"
      provides: "Calls registerInitialSceneScripted() at startup"
      contains: "registerInitialSceneScripted"
  key_links:
    - from: "apps/level_editor/main.cpp"
      to: "GenericFileScene::scriptedGeometryRegistry"
      via: "registerInitialSceneScripted() call at startup"
      pattern: "registerInitialSceneScripted"
    - from: "src/editor/core/EditorRuntimePreviewSession.cpp"
      to: "GenericFileScene::lookupScriptedGeometry"
      via: "rebuild() sets request.buildScriptedGeometry"
      pattern: "lookupScriptedGeometry"
    - from: "EditorRuntimePreviewSession::rebuild"
      to: "RuntimeGameSession::rebuild"
      via: "LevelLoadRequest with populated buildScriptedGeometry"
      pattern: "request\\.buildScriptedGeometry"
---

<objective>
Make the editor preview session run the full runtime scene loading pipeline including scripted geometry, so doors and other code-driven entities appear in the editor exactly as they do in the game.

Purpose: Currently the editor skips scripted geometry during preview — doors spawned by InitialSceneScripted.cpp appear in-game but are invisible in the editor. This breaks editor-runtime parity and makes level design unreliable.

Output: Editor preview sessions that are indistinguishable from the runtime game, including all scripted geometry, physics bodies, doors, and interactions.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/game/scenes/GenericFileScene.h
@src/game/scenes/GenericFileScene.cpp
@src/editor/core/EditorRuntimePreviewSession.h
@src/editor/core/EditorRuntimePreviewSession.cpp
@src/game/scenes/InitialSceneScripted.h
@src/game/scenes/InitialSceneScripted.cpp
@apps/runtime/main.cpp
@apps/level_editor/main.cpp
@src/game/level/LevelLoader.h

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/game/level/LevelLoader.h:
```cpp
struct LevelLoadRequest {
    std::string levelId;
    std::string levelPath;
    std::function<void(MeshLibrary&)> registerAssets;
    std::function<void(class LevelBuilder&)> buildScriptedGeometry;
};
```

From src/game/scenes/GenericFileScene.h:
```cpp
class GenericFileScene : public Scene {
public:
    static void registerScriptedGeometry(const std::string& levelId,
                                          std::function<void(LevelBuilder&)> callback);
    // NOTE: No public lookup method exists yet — Task 1 adds one
};
```

From src/editor/core/EditorRuntimePreviewSession.h:
```cpp
class EditorRuntimePreviewSession {
public:
    void rebuild(const EditorSceneDocument& document, ContentRegistry& content);
    // rebuild() currently creates a LevelLoadRequest but never sets buildScriptedGeometry
};
```

From src/game/scenes/InitialSceneScripted.h:
```cpp
void registerInitialSceneScripted();
```

From src/game/runtime/RuntimeGameSession (rebuild signature):
```cpp
void rebuild(const LevelDef& level, const std::string& levelId,
             const std::string& levelPath, ContentRegistry& content,
             const LevelLoadRequest& request);
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add public lookup API to GenericFileScene scripted geometry registry</name>
  <files>src/game/scenes/GenericFileScene.h, src/game/scenes/GenericFileScene.cpp</files>
  <action>
Add a public static method to GenericFileScene that looks up a scripted geometry callback by levelId, returning it as an optional-like value. The private registry (file-local static in GenericFileScene.cpp) already exists; this just exposes read access.

In `src/game/scenes/GenericFileScene.h`, add after the existing `registerScriptedGeometry` declaration:

```cpp
/// Look up a previously registered scripted geometry callback for the given level ID.
/// Returns an empty std::function if no callback is registered.
static std::function<void(LevelBuilder&)> lookupScriptedGeometry(const std::string& levelId);
```

In `src/game/scenes/GenericFileScene.cpp`, add the implementation after `registerScriptedGeometry`:

```cpp
std::function<void(LevelBuilder&)> GenericFileScene::lookupScriptedGeometry(const std::string& levelId) {
    auto it = scriptedGeometryRegistry().find(levelId);
    if (it != scriptedGeometryRegistry().end()) {
        return it->second;
    }
    return {};
}
```

This mirrors the existing lookup pattern already used inside `GenericFileScene::onEnter()` (lines 71-76) but makes it available externally for the editor.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -20</automated>
  </verify>
  <done>GenericFileScene has a public static lookupScriptedGeometry() method that returns the registered callback for a given levelId, or an empty std::function if none exists. Both pixel-roguelike and level-editor compile cleanly.</done>
</task>

<task type="auto">
  <name>Task 2: Wire editor preview session to use scripted geometry and register callbacks at editor startup</name>
  <files>src/editor/core/EditorRuntimePreviewSession.cpp, apps/level_editor/main.cpp</files>
  <action>
Two changes to achieve full editor-runtime parity:

**A. EditorRuntimePreviewSession::rebuild() — populate buildScriptedGeometry**

In `src/editor/core/EditorRuntimePreviewSession.cpp`:

1. Add include at the top (after existing includes):
   ```cpp
   #include "game/scenes/GenericFileScene.h"
   ```

2. In `rebuild()`, after setting `request.levelId` and `request.levelPath` (line 27-28), and BEFORE the call to `session_.rebuild()` (line 29), add the scripted geometry lookup:
   ```cpp
   // Derive the levelId the same way GenericFileScene does — stem of the scene path
   std::string lookupId = request.levelId;
   if (!document.scenePath().empty()) {
       lookupId = std::filesystem::path(document.scenePath()).stem().string();
   }
   request.buildScriptedGeometry = GenericFileScene::lookupScriptedGeometry(lookupId);
   ```

3. Add the necessary include for filesystem:
   ```cpp
   #include <filesystem>
   ```

This ensures the editor preview uses the exact same scripted geometry as the runtime. The levelId derivation mirrors `GenericFileScene::GenericFileScene()` constructor (line 39 of GenericFileScene.cpp) which uses `std::filesystem::path(scenePath).stem().string()`.

**B. Editor main.cpp — register scripted geometry callbacks**

In `apps/level_editor/main.cpp`:

1. Add include near the top (with other game includes):
   ```cpp
   #include "game/scenes/InitialSceneScripted.h"
   ```

2. Add the registration call early in `main()`, BEFORE any editor UI or preview session could load a scene. Find the section after the namespace block closes and the main() function begins. Add the call right after initial setup but before any ContentRegistry or document loading. A good location is just inside main(), near the top — mirror the runtime's placement (runtime/main.cpp line 77). Look for where variables are first being declared and add:
   ```cpp
   // Register scripted geometry callbacks (mirrors runtime/main.cpp)
   registerInitialSceneScripted();
   ```

   Specifically, place it after `hasValidProjectRoot()` check (if present) or near the top of main() before any document/preview session construction.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -10 && echo "BUILD OK"</automated>
  </verify>
  <done>
    - EditorRuntimePreviewSession::rebuild() looks up scripted geometry from GenericFileScene's registry and populates LevelLoadRequest.buildScriptedGeometry before passing to RuntimeGameSession::rebuild()
    - Editor main.cpp calls registerInitialSceneScripted() at startup, populating the registry
    - When the user hits Play/Preview on initial_scene, doors appear exactly as they do in the game runtime
    - The outliner is unaffected (per user decision: preview session only, not outliner)
    - Scripted geometry is editable during preview but non-persistent (resets on preview restart, per user decision)
  </done>
</task>

</tasks>

<verification>
1. Build both targets: `cmake --build build --target pixel-roguelike level-editor` — no errors
2. Launch the editor, open initial_scene.scene, start a preview session — doors should be visible and interactive
3. Launch the runtime with the same scene — visual output should be identical to editor preview
4. Stop and restart preview in editor — doors reset to their original positions (non-persistent)
5. Check outliner — it should NOT show scripted geometry entities (file-only view)
</verification>

<success_criteria>
- Editor preview session shows all three doors from InitialSceneScripted (Door A, B, C) at the correct positions
- Doors are interactive in editor preview (open/close on interaction)
- Runtime and editor preview are visually indistinguishable for initial_scene
- Build compiles cleanly for all targets (pixel-roguelike, level-editor, procedural-model-viewer)
- No regressions to editor outliner or scene document functionality
</success_criteria>

<output>
After completion, create `.planning/quick/260406-qdy-editor-runtime-parity-shared-scene-loadi/260406-qdy-SUMMARY.md`
</output>
