---
phase: quick
plan: 260405-hxx
type: execute
wave: 1
depends_on: []
files_modified:
  - src/game/runtime/RuntimeGameSession.h
  - src/game/runtime/RuntimeGameSession.cpp
  - src/editor/ui/EditorPerformancePanel.h
  - src/editor/ui/EditorPerformancePanel.cpp
  - src/editor/CMakeLists.txt
  - src/editor/ui/LevelEditorUi.h
  - apps/level_editor/main.cpp
autonomous: true
must_haves:
  truths:
    - "Editor shows per-subsystem timing for each frame in a dedicated Performance panel"
    - "Timings update live during gameplay preview mode"
    - "Performance panel is toggleable from the View menu"
    - "Panel does NOT exist in the runtime game build"
  artifacts:
    - path: "src/editor/ui/EditorPerformancePanel.h"
      provides: "ImGui panel rendering function"
    - path: "src/editor/ui/EditorPerformancePanel.cpp"
      provides: "Performance panel implementation with bar chart"
    - path: "src/game/runtime/RuntimeGameSession.h"
      provides: "Extended RuntimeSessionPerformanceStats with per-subsystem timings"
  key_links:
    - from: "src/editor/ui/EditorPerformancePanel.cpp"
      to: "RuntimeSessionPerformanceStats"
      via: "reads subsystem timings from EditorRuntimePreviewSession::performanceStats()"
      pattern: "performanceStats\\(\\)"
    - from: "apps/level_editor/main.cpp"
      to: "src/editor/ui/EditorPerformancePanel.h"
      via: "calls renderPerformancePanel() in the renderFrame lambda"
      pattern: "renderPerformancePanel"
---

<objective>
Add an editor-only Performance Metrics panel that displays per-subsystem execution times for each frame.

Purpose: Enable profiling of gameplay systems (interaction, checkpoints, physics, inventory, movement, camera) and rendering pipeline passes during editor preview, so the developer can identify performance bottlenecks without external profiling tools.

Output: A toggleable ImGui panel in the editor showing subsystem timings with visual bars, accessible from View > Performance.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/game/runtime/RuntimeGameSession.h
@src/game/runtime/RuntimeGameSession.cpp
@src/editor/core/EditorRuntimePreviewSession.h
@src/editor/ui/LevelEditorUi.h
@src/editor/ui/EditorPanels.h
@src/editor/CMakeLists.txt
@apps/level_editor/main.cpp
@src/engine/rendering/SceneRenderPipeline.h (SceneRenderPipelineStats pattern reference)

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/game/runtime/RuntimeGameSession.h:
```cpp
struct RuntimeSessionPerformanceStats {
    double rebuildMs = 0.0;
    double resetForPlayMs = 0.0;
    double rendererInitMs = 0.0;
    double rendererPrewarmMs = 0.0;
    double lastRenderMs = 0.0;
};

class RuntimeGameSession {
public:
    void tick(float deltaTime, float aspect);
    const RuntimeSessionPerformanceStats& performanceStats() const { return performanceStats_; }
    const SceneRenderPipelineStats& pipelineStats() const { return renderer_.pipelineStats(); }
};
```

From src/editor/core/EditorRuntimePreviewSession.h:
```cpp
class EditorRuntimePreviewSession {
public:
    const RuntimeSessionPerformanceStats& performanceStats() const { return session_.performanceStats(); }
    const SceneRenderPipelineStats& pipelineStats() const { return session_.pipelineStats(); }
};
```

From src/engine/rendering/SceneRenderPipeline.h:
```cpp
struct SceneRenderPipelineStats {
    double totalRenderMs = 0.0;
    double shadowPassMs = 0.0;
    double scenePassMs = 0.0;
    double bloomMs = 0.0;
    double ssaoMs = 0.0;
    double compositeMs = 0.0;
    int drawCalls = 0;
    int objectCount = 0;
    int lightCount = 0;
    int culledCount = 0;
    int shadowCulledCount = 0;
};
```

From src/editor/ui/LevelEditorUi.h:
```cpp
struct EditorUiState {
    // ... existing bool toggles for panels:
    bool showOutliner = true;
    bool showInspector = true;
    bool showAssetBrowser = true;
    bool showEnvironment = true;
    bool showViewport = true;
    bool showBuildOutput = false;
    // ...
};
```

From apps/level_editor/main.cpp (main loop structure):
- Editor has its own while loop, NOT using Application::run()
- renderFrame lambda does all per-frame work
- EditorRuntimePreviewSession.tick() calls RuntimeGameSession::tick() during preview
- View menu has ImGui::MenuItem entries for panel toggles
- Viewport stats overlay already shows some timing info
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Extend RuntimeSessionPerformanceStats with per-subsystem timings and instrument tick()</name>
  <files>src/game/runtime/RuntimeGameSession.h, src/game/runtime/RuntimeGameSession.cpp</files>
  <action>
1. In `RuntimeGameSession.h`, add per-subsystem timing fields to `RuntimeSessionPerformanceStats`:
   ```cpp
   // Per-subsystem tick timings (populated each tick() call)
   double interactionMs = 0.0;
   double checkpointsMs = 0.0;
   double physicsMs = 0.0;
   double inventoryMs = 0.0;
   double movementMs = 0.0;
   double cameraMs = 0.0;
   double totalTickMs = 0.0;
   ```
   Place these after the existing fields (rebuildMs, resetForPlayMs, etc.) with a comment block separating the "setup stats" from the "per-frame tick stats".

2. In `RuntimeGameSession.cpp`, instrument the `tick()` method. The file already uses `std::chrono::steady_clock` (aliased as `Clock`) and has an `elapsedMilliseconds()` helper in an anonymous namespace. Wrap each subsystem call in `tick()` with timing:

   ```cpp
   void RuntimeGameSession::tick(float deltaTime, float aspect) {
       if (!physicsInitialized_) return;

       const Clock::time_point tickStart = Clock::now();

       Clock::time_point t0 = Clock::now();
       updateRuntimeInteraction(registry_, inputSystem_);
       Clock::time_point t1 = Clock::now();
       performanceStats_.interactionMs = elapsedMilliseconds(t0, t1);

       t0 = t1;
       updateRuntimeCheckpoints(registry_, deltaTime, runSession_);
       t1 = Clock::now();
       performanceStats_.checkpointsMs = elapsedMilliseconds(t0, t1);

       t0 = t1;
       physics_.update(registry_, deltaTime);
       t1 = Clock::now();
       performanceStats_.physicsMs = elapsedMilliseconds(t0, t1);

       ContentRegistry* content = nullptr;
       if (registry_.ctx().contains<ContentRegistry*>()) {
           content = registry_.ctx().get<ContentRegistry*>();
       }
       t0 = Clock::now();
       if (content != nullptr) {
           updateRuntimeInventory(registry_, inputSystem_, runSession_, *content);
       }
       t1 = Clock::now();
       performanceStats_.inventoryMs = elapsedMilliseconds(t0, t1);

       t0 = t1;
       updateRuntimePlayerMovement(registry_, inputSystem_, physics_, deltaTime);
       t1 = Clock::now();
       performanceStats_.movementMs = elapsedMilliseconds(t0, t1);

       t0 = t1;
       updateRuntimeCamera(registry_, inputSystem_, aspect, deltaTime);
       t1 = Clock::now();
       performanceStats_.cameraMs = elapsedMilliseconds(t0, t1);

       performanceStats_.totalTickMs = elapsedMilliseconds(tickStart, t1);
   }
   ```

   This preserves the exact same execution order and logic, just adding timing around each call. The overhead is negligible (6 chrono::now() calls, ~50ns each).
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>RuntimeSessionPerformanceStats has 7 new timing fields. RuntimeGameSession::tick() records per-subsystem timings each frame. Both executables compile cleanly.</done>
</task>

<task type="auto">
  <name>Task 2: Create EditorPerformancePanel and wire into editor UI</name>
  <files>src/editor/ui/EditorPerformancePanel.h, src/editor/ui/EditorPerformancePanel.cpp, src/editor/CMakeLists.txt, src/editor/ui/LevelEditorUi.h, apps/level_editor/main.cpp</files>
  <action>
1. Create `src/editor/ui/EditorPerformancePanel.h`:
   ```cpp
   #pragma once

   struct RuntimeSessionPerformanceStats;
   struct SceneRenderPipelineStats;

   void renderPerformancePanel(const RuntimeSessionPerformanceStats& perf,
                               const SceneRenderPipelineStats& pipeline,
                               bool isPreviewActive,
                               bool* open);
   ```
   Keep it minimal. Forward-declare the stats structs to avoid pulling game headers into the editor UI header.

2. Create `src/editor/ui/EditorPerformancePanel.cpp`:
   - Include `imgui.h`, `RuntimeGameSession.h` (for `RuntimeSessionPerformanceStats`), `SceneRenderPipeline.h` (for `SceneRenderPipelineStats`), and the panel header.
   - Implement `renderPerformancePanel()`:
     - Use `beginCompactEditorPanelWindow("Performance", open)` to match the editor's existing panel pattern (defined in LevelEditorUi.h).
     - If `!isPreviewActive`, show a centered text: "Enter Play Preview to see system timings" and return early (the tick stats are only meaningful during preview).
     - **Gameplay Systems section** (`ImGui::SeparatorText("Gameplay Systems")`):
       - Show a table with columns: System | Time (ms) | Bar
       - Rows for: Interaction, Checkpoints, Physics, Inventory, Movement, Camera
       - For each row, render the ms value right-aligned and a colored horizontal bar (`ImGui::GetWindowDrawList()->AddRectFilled`) proportional to the value. Use a consistent max scale (e.g., clamp bar width to represent 0-2ms range, auto-scaling if any value exceeds 2ms). Use a warm amber color (IM_COL32(220, 170, 60, 200)) for the bars.
       - Show "Total Tick: X.XX ms" below the table.
     - **Render Pipeline section** (`ImGui::SeparatorText("Render Pipeline")`):
       - Same table layout for: Shadow, Scene, SSAO, Bloom, Composite
       - Show "Total Render: X.XX ms" below.
       - Show draw call and object count stats on one line: "Draws: N  Objects: N  Lights: N  Culled: N"
     - **Frame Summary section** (`ImGui::SeparatorText("Frame")`):
       - Show total frame time and FPS using ImGui::GetIO().Framerate.
       - Show a stacked summary: "Tick: X.XX + Render: X.XX + Other: X.XX = Total: X.XX ms" where Other = frameMs - tickMs - renderMs.

   The panel should be compact and information-dense -- no unnecessary padding. Use `ImGui::Text` and `ImGui::GetWindowDrawList()` for the bar rendering, following the pattern used by the viewport stats overlay in main.cpp.

3. Add `ui/EditorPerformancePanel.cpp` to the `add_library(editor STATIC ...)` list in `src/editor/CMakeLists.txt`. Place it alphabetically near the other `ui/Editor*.cpp` entries.

4. In `src/editor/ui/LevelEditorUi.h`, add a new boolean to `EditorUiState`:
   ```cpp
   bool showPerformance = false;
   ```
   Place it after `showBuildOutput`.

5. In `apps/level_editor/main.cpp`:
   - Add `#include "editor/ui/EditorPerformancePanel.h"` with the other editor UI includes.
   - In the View menu (search for `ImGui::MenuItem("Viewport Stats"`), add a new menu item:
     ```cpp
     ImGui::MenuItem("Performance", nullptr, &ui.showPerformance);
     ```
     Place it near the "Viewport Stats" entry since both are performance-related.
   - In the renderFrame lambda, after the existing panel rendering calls (environment panel, asset browser, inspector, outliner), add:
     ```cpp
     if (ui.showPerformance) {
         renderPerformancePanel(
             runtimePreviewSession.performanceStats(),
             runtimePreviewSession.pipelineStats(),
             ui.playPreview,
             &ui.showPerformance);
     }
     ```
     Place this near the other panel render calls (search for `renderEnvironmentPanel` or `renderInspector` to find the right location).
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>Performance panel compiles into the editor target. The panel is toggleable from the View menu. When play preview is active, it shows per-subsystem tick timings, render pipeline timings, and frame summary. When not in preview, it shows a prompt message. The runtime game executable does not include or link the panel code (it only links gameplay, not editor).</done>
</task>

</tasks>

<verification>
1. Build succeeds for all three targets: `cmake --build build --target pixel-roguelike level-editor procedural-model-viewer`
2. Launch `level-editor`, open View menu, confirm "Performance" menu item exists
3. Click "Performance" -- panel appears with "Enter Play Preview to see system timings"
4. Enter Play Preview (Cmd+P) -- panel now shows live per-subsystem timings
5. Verify the runtime game (`pixel-roguelike`) does NOT contain performance panel code (it only links `gameplay`, not `editor`)
</verification>

<success_criteria>
- Performance panel renders in the editor with per-subsystem tick timings and render pipeline stats
- Timings update every frame during play preview
- Panel is toggled via View > Performance menu
- No performance panel code in the runtime game build
- No measurable performance regression from the chrono instrumentation in tick()
</success_criteria>

<output>
After completion, create `.planning/quick/260405-hxx-implement-editor-only-performance-metric/260405-hxx-SUMMARY.md`
</output>
