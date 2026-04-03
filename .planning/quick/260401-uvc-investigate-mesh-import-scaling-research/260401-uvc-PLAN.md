---
phase: quick
plan: 260401-uvc
type: execute
wave: 1
depends_on: []
files_modified:
  - src/engine/rendering/assets/AssimpLoader.cpp
  - assets/scenes/country_house.scene
  - src/game/scenes/GenericFileScene.cpp
autonomous: true
requirements: []
must_haves:
  truths:
    - "FBX meshes import at correct meter scale without manual 0.01 workarounds"
    - "country_house scene renders at the same visual size as before the change"
    - "institutional_room doors render at the same visual size as before the change"
    - "glTF meshes (arch.glb, pillar.glb, etc.) are unaffected"
  artifacts:
    - path: "src/engine/rendering/assets/AssimpLoader.cpp"
      provides: "AI_CONFIG_FBX_CONVERT_TO_M on both Importer instances"
      contains: "AI_CONFIG_FBX_CONVERT_TO_M"
    - path: "assets/scenes/country_house.scene"
      provides: "Scene file with 1.0 scale for FBX meshes"
    - path: "src/game/scenes/GenericFileScene.cpp"
      provides: "Scripted geometry with 1.0 scale for wood_door"
  key_links:
    - from: "src/engine/rendering/assets/AssimpLoader.cpp"
      to: "Assimp::Importer"
      via: "SetPropertyBool before ReadFile"
      pattern: "SetPropertyBool.*AI_CONFIG_FBX_CONVERT_TO_M"
---

<objective>
Fix FBX mesh import scaling so centimeter-authored FBX files are automatically converted to meters during Assimp import, eliminating manual 0.01 scale workarounds in scene files and scripted geometry.

Purpose: FBX files default to centimeters. The engine uses meters. Currently every FBX mesh placement requires a manual scale=(0.01,0.01,0.01) workaround. Assimp's built-in AI_CONFIG_FBX_CONVERT_TO_M flag handles this conversion at the vertex level during import.

Output: AssimpLoader applies cm-to-m conversion; scene files and scripted geometry use natural 1.0 scale for FBX meshes.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/engine/rendering/assets/AssimpLoader.cpp
@src/engine/rendering/assets/AssimpLoader.h
@src/game/scenes/GenericFileScene.cpp
@assets/scenes/country_house.scene
@.planning/quick/260401-uvc-investigate-mesh-import-scaling-research/260401-uvc-RESEARCH.md
</context>

<interfaces>
<!-- Key functions in AssimpLoader.cpp that need modification -->

From src/engine/rendering/assets/AssimpLoader.cpp:
```cpp
// Line 295-319 — loadRaw: single Assimp::Importer, ReadFile with kAssimpImportFlags
RawMeshData AssimpLoader::loadRaw(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, kAssimpImportFlags);
    // ...
}

// Line 321-342 — loadRawMulti: second Assimp::Importer, ReadFile with kAssimpImportFlags
std::vector<NamedRawMeshData> AssimpLoader::loadRawMulti(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, kAssimpImportFlags);
    // ...
}
```

From src/game/scenes/GenericFileScene.cpp:
```cpp
// Line 36-41 — metal door placement with 0.01 scale
auto metalDoor = builder.addMesh("wood_door",
    glm::vec3(0.0f, 0.0f, 5.95f),
    glm::vec3(0.01f, 0.01f, 0.01f),  // needs to become 1.0
    ...);

// Line 57-61 — chained door placement with 0.01 scale
auto chainedDoor = builder.addMesh("wood_door",
    glm::vec3(2.5f, 0.0f, 5.95f),
    glm::vec3(0.01f, 0.01f, 0.01f),  // needs to become 1.0
    ...);
```
</interfaces>

<tasks>

<task type="auto">
  <name>Task 1: Enable FBX cm-to-m conversion in AssimpLoader and clear stale cache</name>
  <files>src/engine/rendering/assets/AssimpLoader.cpp</files>
  <action>
  In AssimpLoader.cpp:

  1. Add `#include <assimp/config.h>` to the include block (after the existing assimp includes, before glm).

  2. In `AssimpLoader::loadRaw()` (line ~296), add `importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);` BETWEEN the `Assimp::Importer importer;` declaration and the `importer.ReadFile()` call.

  3. In `AssimpLoader::loadRawMulti()` (line ~322), add the identical `importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);` BETWEEN the `Assimp::Importer importer;` declaration and the `importer.ReadFile()` call.

  This flag tells Assimp's FBX importer to read the file's UnitScaleFactor metadata and convert vertex positions from centimeters to meters. It does NOT blindly divide by 100 -- it reads the actual scale factor from the FBX header. glTF imports go through GltfLoader, not AssimpLoader, so they are unaffected.

  4. Delete all FBX-sourced cached mesh files to prevent stale data. Run:
     `rm .cache/meshes/country_house* .cache/meshes/wood_door* .cache/meshes/country_house_door*`
     The glTF caches (arch, pillar, hand_low_poly, gothic_door_static) must NOT be deleted -- they are correct.
  </action>
  <verify>
    <automated>grep -n "AI_CONFIG_FBX_CONVERT_TO_M" src/engine/rendering/assets/AssimpLoader.cpp | wc -l | xargs test 2 -eq</automated>
  </verify>
  <done>Both Assimp::Importer instances in AssimpLoader.cpp have SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true) set before ReadFile. The assimp/config.h header is included. FBX mesh cache files are deleted.</done>
</task>

<task type="auto">
  <name>Task 2: Remove 0.01 scale workarounds from scene files and scripted geometry</name>
  <files>assets/scenes/country_house.scene, src/game/scenes/GenericFileScene.cpp</files>
  <action>
  In `assets/scenes/country_house.scene`:

  1. Update the comment on line 4 to remove the "scale 0.01 converts to meters" note. Replace with a note like "FBX auto-converted to meters by AssimpLoader" or remove the comment entirely.

  2. Change ALL `0.01 0.01 0.01` scale values to `1.0 1.0 1.0` for every mesh line. There are 10 mesh lines (lines 8-12 for house structure, lines 15-17 for doors) that all use `0.01 0.01 0.01`. Each one becomes `1.0 1.0 1.0`.

  In `src/game/scenes/GenericFileScene.cpp`:

  3. In `buildInstitutionalRoomGeometry()`, change the two wood_door addMesh calls:
     - Line 38: `glm::vec3(0.01f, 0.01f, 0.01f)` becomes `glm::vec3(1.0f, 1.0f, 1.0f)`
     - Line 59: `glm::vec3(0.01f, 0.01f, 0.01f)` becomes `glm::vec3(1.0f, 1.0f, 1.0f)`

  Do NOT touch any other mesh placements in these files -- only FBX-loaded meshes had the 0.01 workaround. The procedural meshes (prison_wall, prison_floor, etc.) and glTF meshes (arch, pillar) already use correct 1.0 scale.
  </action>
  <verify>
    <automated>! grep -q "0\.01 0\.01 0\.01" assets/scenes/country_house.scene && ! grep -q "0\.01f, 0\.01f, 0\.01f" src/game/scenes/GenericFileScene.cpp && echo "PASS"</automated>
  </verify>
  <done>No 0.01 scale workarounds remain in country_house.scene or GenericFileScene.cpp. FBX meshes use 1.0 scale. Procedural and glTF meshes are untouched.</done>
</task>

<task type="auto">
  <name>Task 3: Build and verify meshes render at correct scale</name>
  <files></files>
  <action>
  1. Build the project: `cd build && cmake --build . --target pixel-roguelike level-editor 2>&1`
     Confirm zero errors and zero warnings related to AssimpLoader or config.h.

  2. Verify the fix is correct by checking the compiled binary contains the new import path. The build succeeding confirms the assimp/config.h include resolves and AI_CONFIG_FBX_CONVERT_TO_M is a valid symbol.

  If the build fails due to AI_CONFIG_FBX_CONVERT_TO_M not being found, fall back to using `AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY` with `aiProcess_GlobalScale` flag instead (see RESEARCH.md alternatives). But this should not happen -- the research confirmed the symbol exists in the installed Assimp 6.0.2 headers.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target pixel-roguelike 2>&1 | tail -5</automated>
  </verify>
  <done>Project builds successfully with FBX cm-to-m conversion enabled. No compilation errors from the assimp config include or SetPropertyBool calls.</done>
</task>

</tasks>

<verification>
1. `grep -c "AI_CONFIG_FBX_CONVERT_TO_M" src/engine/rendering/assets/AssimpLoader.cpp` returns 2 (one per importer instance)
2. `grep -c "0.01 0.01 0.01" assets/scenes/country_house.scene` returns 0
3. `grep -c "0.01f, 0.01f, 0.01f" src/game/scenes/GenericFileScene.cpp` returns 0
4. Project builds without errors
5. No FBX mesh cache files exist in .cache/meshes/ (country_house*, wood_door*)
</verification>

<success_criteria>
- AssimpLoader applies AI_CONFIG_FBX_CONVERT_TO_M to both loadRaw() and loadRawMulti()
- country_house.scene uses 1.0 scale for all FBX meshes
- GenericFileScene.cpp uses 1.0 scale for wood_door placements
- Stale FBX mesh cache cleared
- Project compiles cleanly
</success_criteria>

<output>
After completion, create `.planning/quick/260401-uvc-investigate-mesh-import-scaling-research/260401-uvc-SUMMARY.md`
</output>
