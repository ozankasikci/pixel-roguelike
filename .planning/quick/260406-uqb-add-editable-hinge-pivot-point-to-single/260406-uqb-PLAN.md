---
phase: quick
plan: 260406-uqb
type: execute
wave: 1
depends_on: []
files_modified:
  - src/game/level/LevelDef.h
  - src/game/level/LevelDef.cpp
  - src/game/prefabs/GameplayPrefabData.h
  - src/game/level/LevelBuilder.cpp
  - src/game/prefabs/GameplayPrefabs.cpp
  - src/editor/ui/inspectors/SingleDoorInspector.cpp
  - src/editor/scene/EditorPreviewWorld.cpp
autonomous: true
must_haves:
  truths:
    - "Door hinge pivot is editable in the editor inspector"
    - "Custom hinge pivot values survive save/load round-trip"
    - "Editor preview renders door leaf at correct hinge position from the editable field"
    - "Runtime gameplay uses the scene-defined hinge pivot instead of hardcoded offset"
  artifacts:
    - path: "src/game/level/LevelDef.h"
      provides: "hingePivot field on LevelSingleDoorPlacement"
      contains: "hingePivot"
    - path: "src/game/prefabs/GameplayPrefabData.h"
      provides: "hingePivot field on SingleDoorSpawnSpec"
      contains: "hingePivot"
    - path: "src/editor/ui/inspectors/SingleDoorInspector.cpp"
      provides: "Hinge Pivot vec3 editor row"
      contains: "Hinge Pivot"
  key_links:
    - from: "src/game/level/LevelDef.cpp"
      to: "LevelSingleDoorPlacement.hingePivot"
      via: "parser token 'hinge_pivot' and serializer"
      pattern: "hinge_pivot"
    - from: "src/game/level/LevelBuilder.cpp"
      to: "SingleDoorSpawnSpec.hingePivot"
      via: "field copy from placement to spec"
      pattern: "spec\\.hingePivot"
    - from: "src/game/prefabs/GameplayPrefabs.cpp"
      to: "spec.hingePivot"
      via: "reads hingePivot instead of hardcoded offset"
      pattern: "spec\\.hingePivot"
    - from: "src/editor/scene/EditorPreviewWorld.cpp"
      to: "placement.hingePivot"
      via: "reads from door payload in both rebuild and syncTransforms"
      pattern: "door\\.hingePivot|placement\\.hingePivot"
---

<objective>
Add an editable hinge pivot point field to SingleDoor so the hinge offset is data-driven from the .scene file rather than hardcoded in three places. The field appears in the editor inspector, serializes to the scene format, and flows through to both editor preview and runtime gameplay.

Purpose: Allows per-door hinge adjustment without recompiling; eliminates the hardcoded (-0.45, 0, 0.04) magic vector from editor and runtime code.
Output: hingePivot field on LevelSingleDoorPlacement and SingleDoorSpawnSpec, editor inspector row, parser/serializer support, updated editor preview and runtime prefab spawning.
</objective>

<execution_context>
@.planning/quick/260406-uqb-add-editable-hinge-pivot-point-to-single/260406-uqb-PLAN.md
</execution_context>

<context>
@CLAUDE.md
@src/game/level/LevelDef.h (lines 110-127 — LevelSingleDoorPlacement struct)
@src/game/level/LevelDef.cpp (lines 962-1042 — single_door parser; lines 1445-1475 — serializer)
@src/game/prefabs/GameplayPrefabData.h (lines 12-27 — SingleDoorSpawnSpec struct)
@src/game/level/LevelBuilder.cpp (lines 280-301 — addSingleDoor copies placement to spec)
@src/game/prefabs/GameplayPrefabs.cpp (lines 163-205 — spawnSingleDoor uses hardcoded offset)
@src/editor/ui/inspectors/SingleDoorInspector.cpp (lines 109-128 — Yaw and subsequent inspector rows)
@src/editor/scene/EditorPreviewWorld.cpp (lines 239-267 — rebuild SingleDoor case; lines 394-427 — syncTransforms SingleDoor case)

<interfaces>
From src/editor/ui/LevelEditorUi.h:
```cpp
bool editVec3(const char* label, glm::vec3& value, float speed = 0.01f);
```

From src/editor/ui/inspectors/SingleDoorInspector.cpp pattern:
```cpp
auto itemBefore = document.captureState();
trackSceneItem(itemBefore, "Change Description",
    renderInspectorPropertyRow("Label", [&]() {
        return ImGui::DragFloat("##value", &door.field, ...);
    }));
```

From src/editor/scene/EditorPreviewWorld.cpp syncTransforms:
```cpp
// object is const EditorSceneObject* from document.findObject()
// For SingleDoor, access payload via:
const auto& door = std::get<LevelSingleDoorPlacement>(object->payload);
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add hingePivot field to data structs, parser, and serializer</name>
  <files>
    src/game/level/LevelDef.h
    src/game/level/LevelDef.cpp
    src/game/prefabs/GameplayPrefabData.h
    src/game/level/LevelBuilder.cpp
  </files>
  <action>
  1. In `src/game/level/LevelDef.h`, add to `LevelSingleDoorPlacement` (after `frameTint`, before `nodeId`):
     ```cpp
     glm::vec3 hingePivot{-0.45f, 0.0f, 0.04f};
     ```

  2. In `src/game/prefabs/GameplayPrefabData.h`, add to `SingleDoorSpawnSpec` (after `frameTint`, before the closing brace):
     ```cpp
     glm::vec3 hingePivot{-0.45f, 0.0f, 0.04f};
     ```

  3. In `src/game/level/LevelDef.cpp` PARSER (around line 1033, before the `parseNodeMetadata` call in the single_door token loop), add:
     ```cpp
     if (tokens[index] == "hinge_pivot") {
         if (index + 3 >= tokens.size()) throwParseError(path, lineNumber, "missing hinge_pivot values");
         if (!tryParseFloatToken(tokens[index + 1], placement.hingePivot.x))
             throwParseError(path, lineNumber, "invalid hinge_pivot x");
         if (!tryParseFloatToken(tokens[index + 2], placement.hingePivot.y))
             throwParseError(path, lineNumber, "invalid hinge_pivot y");
         if (!tryParseFloatToken(tokens[index + 3], placement.hingePivot.z))
             throwParseError(path, lineNumber, "invalid hinge_pivot z");
         index += 4;
         continue;
     }
     ```

  4. In `src/game/level/LevelDef.cpp` SERIALIZER (around line 1470, before `appendNodeMetadata`), add:
     ```cpp
     if (d.hingePivot != glm::vec3(-0.45f, 0.0f, 0.04f))
         out << " hinge_pivot " << formatFloat(d.hingePivot.x) << ' '
             << formatFloat(d.hingePivot.y) << ' ' << formatFloat(d.hingePivot.z);
     ```
     Only writes non-default values so existing scene files remain untouched.

  5. In `src/game/level/LevelBuilder.cpp` `addSingleDoor()` (around line 295, after `spec.frameTint`), add:
     ```cpp
     spec.hingePivot = placement.hingePivot;
     ```
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>hingePivot field exists on both structs, parses from and serializes to .scene format (only when non-default), and flows from LevelBuilder into the spawn spec. Project compiles cleanly.</done>
</task>

<task type="auto">
  <name>Task 2: Replace hardcoded offsets in editor preview and runtime prefab, add inspector row</name>
  <files>
    src/game/prefabs/GameplayPrefabs.cpp
    src/editor/scene/EditorPreviewWorld.cpp
    src/editor/ui/inspectors/SingleDoorInspector.cpp
  </files>
  <action>
  1. In `src/game/prefabs/GameplayPrefabs.cpp` line 180, replace:
     ```cpp
     const glm::vec3 localHingeOffset(-0.45f, 0.0f, 0.04f);
     ```
     with:
     ```cpp
     const glm::vec3& localHingeOffset = spec.hingePivot;
     ```
     The rest of the function (hingeWorldPos computation, centerOffsetFromHinge, DoorLeafComponent setup) all read from `localHingeOffset` so no other changes needed in this file.

  2. In `src/editor/scene/EditorPreviewWorld.cpp` rebuild() (line 256), replace:
     ```cpp
     const glm::vec3 localHingeOffset(-0.45f, 0.0f, 0.04f);
     ```
     with:
     ```cpp
     const glm::vec3& localHingeOffset = placement.hingePivot;
     ```
     The variable `placement` is already `LevelSingleDoorPlacement` from `std::get` at line 240.

  3. In `src/editor/scene/EditorPreviewWorld.cpp` syncTransforms() SingleDoor case (line 394-427), two changes:
     a. After `case EditorSceneObjectKind::SingleDoor: {` and `constexpr float kDoorScale = 0.22f;`, extract the door data from the payload:
        ```cpp
        const auto& door = std::get<LevelSingleDoorPlacement>(object->payload);
        ```
     b. Replace the hardcoded offset on line 399:
        ```cpp
        const glm::vec3 localHingeOffset(-0.45f, 0.0f, 0.04f);
        ```
        with:
        ```cpp
        const glm::vec3& localHingeOffset = door.hingePivot;
        ```

  4. In `src/editor/ui/inspectors/SingleDoorInspector.cpp`, after the Yaw section (after line 114), add a Hinge Pivot editor row:
     ```cpp
     // Hinge pivot
     itemBefore = document.captureState();
     trackSceneItem(itemBefore, "Change Hinge Pivot",
         renderInspectorPropertyRow("Hinge Pivot", [&]() {
             return editVec3("##value", door.hingePivot);
         }));
     ```
     This follows the exact same pattern used by all other inspector rows. The `door` variable is the mutable `LevelSingleDoorPlacement&` already available in scope. Place it between Yaw and Open Angle sections since it's a spatial property.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>All three hardcoded (-0.45, 0, 0.04) offsets replaced with reads from hingePivot field. Editor inspector shows "Hinge Pivot" vec3 row for SingleDoor entities. Full project compiles with no warnings related to these changes.</done>
</task>

<task type="auto">
  <name>Task 3: Verify no remaining hardcoded hinge offsets and validate round-trip</name>
  <files></files>
  <action>
  1. Grep the entire src/ tree for the old hardcoded values to confirm none remain:
     ```bash
     grep -rn "\-0\.45.*0\.04\|localHingeOffset(-" src/
     ```
     Expected: zero matches (all replaced with reads from hingePivot).

  2. Grep for `hingePivot` across the codebase to confirm consistent usage:
     ```bash
     grep -rn "hingePivot" src/
     ```
     Expected: hits in LevelDef.h, LevelDef.cpp (parser + serializer), GameplayPrefabData.h, LevelBuilder.cpp, GameplayPrefabs.cpp, EditorPreviewWorld.cpp (rebuild + syncTransforms), SingleDoorInspector.cpp.

  3. Run the existing test suite to confirm nothing is broken:
     ```bash
     cd build && ctest --output-on-failure
     ```
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && grep -rn "\-0\.45f.*0\.04f" src/ | grep -v "hingePivot{" ; echo "Exit: $?"</automated>
  </verify>
  <done>No hardcoded hinge offsets remain outside the default initializer. All tests pass. The hingePivot field is consistently used across editor, runtime, parser, and serializer.</done>
</task>

</tasks>

<verification>
1. Build succeeds: `cmake --build build --target pixel-roguelike level-editor`
2. No hardcoded hinge offsets: `grep -rn "\-0\.45f.*0\.04f" src/` only shows the default initializer in LevelDef.h and GameplayPrefabData.h
3. Tests pass: `cd build && ctest --output-on-failure`
4. Round-trip: Open a scene with a SingleDoor in the editor, change Hinge Pivot in inspector, save, reload -- values preserved
</verification>

<success_criteria>
- hingePivot field on LevelSingleDoorPlacement with default {-0.45, 0.0, 0.04}
- hingePivot field on SingleDoorSpawnSpec with same default
- Parser handles `hinge_pivot x y z` token in single_door records
- Serializer writes `hinge_pivot` only when non-default
- LevelBuilder copies hingePivot from placement to spec
- GameplayPrefabs reads spec.hingePivot instead of hardcoded vector
- EditorPreviewWorld rebuild() reads placement.hingePivot instead of hardcoded vector
- EditorPreviewWorld syncTransforms() reads door.hingePivot from payload instead of hardcoded vector
- SingleDoorInspector shows editable "Hinge Pivot" vec3 row
- All existing tests pass
</success_criteria>

<output>
After completion, create `.planning/quick/260406-uqb-add-editable-hinge-pivot-point-to-single/260406-uqb-SUMMARY.md`
</output>
