---
phase: 19-refactor-singledoor-into-multi-part-group-architecture-with-
verified: 2026-04-07T00:30:00Z
status: passed
score: 18/18 must-haves verified
re_verification: false
gaps: []
human_verification:
  - test: "Open a scene with door groups in the level editor, click a door leaf mesh, verify DoorGroup gets selected (not the leaf mesh)"
    expected: "Selecting a door leaf child mesh in the viewport promotes selection to its parent DoorGroup"
    why_human: "Click-bubble-to-parent logic is verified in code, but runtime viewport selection behavior requires a running editor"
  - test: "Inspect a DoorGroup in the editor — verify all fields (name, position, yaw, open angle, open duration, interact distance, interact dot, locked, locked prompt) are editable"
    expected: "DoorGroupInspector renders and all property rows are interactable with undo/redo"
    why_human: "ImGui panel correctness requires visual inspection"
  - test: "Open a scene with a door group, move the door group using gizmo, verify leaf mesh re-renders at correct pivot-aware position"
    expected: "Leaf mesh follows pivot-aware transform when door group is repositioned"
    why_human: "syncTransforms pivot recalculation path is code-verified but requires visual confirmation in the editor"
---

# Phase 19: Refactor SingleDoor into Multi-Part Group Architecture Verification Report

**Phase Goal:** Refactor SingleDoor into multi-part group architecture with pivot-based rotation — replace the monolithic LevelSingleDoorPlacement with a door group containing child mesh placements, add pivot property to mesh placements, extract shared pivot math helper, update editor to use DoorGroup object kind with full inspector and selection support

**Verified:** 2026-04-07T00:30:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | LevelDoorGroupPlacement struct exists as the door group data type | VERIFIED | `src/game/level/LevelDef.h` lines 111-123: struct with name, position, yawDegrees, openAngle, openDuration, interactDistance, interactDotThreshold, locked, lockedPrompt, nodeId, parentNodeId |
| 2 | LevelMeshPlacement has an optional pivot field for hinge-based rotation | VERIFIED | `src/game/level/LevelDef.h` line 57: `std::optional<glm::vec3> pivot;` with comment "hinge pivot for door leaf meshes" |
| 3 | Scene parser reads door_group keyword and pivot token on mesh lines | VERIFIED | `src/game/level/LevelDef.cpp`: `if (kind == "door_group")` at line 971; `if (tokens[index] == "pivot")` at line 721 |
| 4 | Scene serializer writes door_group and pivot in correct format | VERIFIED | `src/game/level/LevelDef.cpp` lines 1262-1266: pivot serialization on mesh lines; line 1423: door_group serializer block |
| 5 | resolveLevelHierarchy handles DoorGroup nodes for transform inheritance | VERIFIED | `src/game/level/LevelDef.cpp`: LevelNodeRef::Kind::DoorGroup at lines 1061, 1112, 1218 — both localMatrixFor and write-back switch covered |
| 6 | Shared makePivotLeafModel helper eliminates duplicated pivot math | VERIFIED | `src/game/prefabs/GameplayPrefabs.h` lines 16-19 declaration; `GameplayPrefabs.cpp` lines 62-76: full matrix computation (translate to hinge, rotate, translate back, scale) |
| 7 | Runtime addDoorGroup spawns frame mesh + leaf with DoorLeafComponent + door root with DoorComponent | VERIFIED | `src/game/level/LevelBuilder.cpp` lines 282-398: frame via addMesh, leaf with modelOverride + DoorLeafComponent, door root with DoorComponent + InteractableComponent (or InteractableComponent only when locked) |
| 8 | initial_scene.scene uses door_group format instead of single_door | VERIFIED | 3 door_group lines (n_doorA_group, n_doorB_group, n_doorC_group) + 3 pivot-bearing leaf mesh lines; zero single_door occurrences |
| 9 | LevelSingleDoorPlacement, spawnSingleDoor, addSingleDoor are deleted | VERIFIED | grep over all of src/ returns zero matches for LevelSingleDoorPlacement, spawnSingleDoor, SingleDoorSpawnSpec |
| 10 | EditorSceneObjectKind::DoorGroup replaces SingleDoor in the enum and all switch/visit sites | VERIFIED | `EditorSceneDocument.h` line 22: DoorGroup in enum; switch/visit sites updated in EditorSceneDocument.cpp (applyWorldTransform, localTransformMatrix, supportsParenting, toLevelDef, editorSceneObjectKindName, editorSceneObjectLabel), EditorPreviewWorld.cpp (rebuild, syncTransforms), EditorSelectionSystem.cpp (selectionPriority, buildEditorSelectionHandles), EditorViewportInteraction.cpp (isViewportSelectableKind, applyGizmoToSelectedObject×2), EditorCommand.cpp (makeLevelDefFromState) |
| 11 | EditorSceneObjectPayload variant uses LevelDoorGroupPlacement instead of LevelSingleDoorPlacement | VERIFIED | `EditorSceneDocument.h` lines 25-33: variant ends with LevelDoorGroupPlacement |
| 12 | addDoorGroup replaces addSingleDoor in EditorSceneDocument | VERIFIED | `EditorSceneDocument.h` line 82: addDoorGroup declaration; `EditorSceneDocument.cpp` line 221: implementation; loadFromSceneFile iterates level.doors and calls addDoorGroup |
| 13 | Clicking a door child mesh selects the parent DoorGroup | VERIFIED | `EditorViewportInteraction.cpp` lines 216-224: applySelectionHit checks if clicked Mesh's parent is DoorGroup and redirects selection to parent |
| 14 | EditorPreviewWorld renders door groups: frame as normal mesh, leaf with pivot-aware model from makePivotLeafModel | VERIFIED | `EditorPreviewWorld.cpp` lines 176-189 (rebuild) and 347-360 (syncTransforms): pivot.has_value() check triggers makePivotLeafModel override |
| 15 | EditorDoorLeafTag is deleted — leaf identified by pivot.has_value() | VERIFIED | grep over all of src/ returns zero matches for EditorDoorLeafTag; EditorPreviewWorld.h has no such struct |
| 16 | DoorGroupInspector shows group properties (position, yaw, openAngle, etc.) | VERIFIED | `DoorGroupInspector.cpp`: name, position, yaw, open angle (SliderFloat 0-180), open duration, interact distance, interact dot threshold, locked checkbox, locked prompt (conditional) |
| 17 | MeshInspector shows optional pivot row when pivot.has_value() | VERIFIED | `MeshInspector.cpp` lines 82-93: pivot row rendered only when mesh.pivot.has_value() |
| 18 | LevelDef.doors field uses new type, LevelLoader calls addDoorGroup per door | VERIFIED | `LevelDef.h` line 136: `std::vector<LevelDoorGroupPlacement> doors`; `LevelLoader.cpp` lines 56-91: doorChildNodeIds skip set + addDoorGroup loop |

**Score:** 18/18 truths verified

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `src/game/level/LevelDef.h` | VERIFIED | LevelDoorGroupPlacement struct at line 111; pivot field on LevelMeshPlacement at line 57; doors vector with new type at line 136 |
| `src/game/level/LevelDef.cpp` | VERIFIED | door_group parser (line 971), pivot parser (line 721), door_group serializer (line 1423), pivot serializer (line 1262), DoorGroup in resolveLevelHierarchy (lines 1061, 1112, 1218) |
| `src/game/prefabs/GameplayPrefabs.h` | VERIFIED | makePivotLeafModel declaration at line 16; computeHingeWorldPos declaration at line 21; no spawnSingleDoor |
| `src/game/prefabs/GameplayPrefabs.cpp` | VERIFIED | Full implementations of both helpers at lines 62-86 |
| `src/game/prefabs/GameplayPrefabData.h` | VERIFIED | No SingleDoorSpawnSpec; only CheckpointSpawnSpec and DoubleDoorSpawnSpec remain |
| `src/game/level/LevelBuilder.h` | VERIFIED | addDoorGroup declaration at line 60; no addSingleDoor |
| `src/game/level/LevelBuilder.cpp` | VERIFIED | addDoorGroup implementation at lines 282-398; full leaf/frame/DoorLeafComponent/DoorComponent spawning logic |
| `src/game/level/LevelLoader.cpp` | VERIFIED | doorChildNodeIds skip set (lines 58-65), addDoorGroup call loop (lines 89-91) |
| `assets/scenes/initial_scene.scene` | VERIFIED | 3 door_group entries (n_doorA_group, n_doorB_group, n_doorC_group); 3 pivot leaf mesh entries; 0 single_door entries |
| `src/editor/scene/EditorSceneDocument.h` | VERIFIED | DoorGroup in enum at line 22; LevelDoorGroupPlacement in variant at line 33; addDoorGroup at line 82 |
| `src/editor/scene/EditorSceneDocument.cpp` | VERIFIED | All visit/switch sites updated — 6 constexpr branches + 4 enum cases |
| `src/editor/scene/EditorPreviewWorld.h` | VERIFIED | No EditorDoorLeafTag struct |
| `src/editor/scene/EditorPreviewWorld.cpp` | VERIFIED | DoorGroup rebuild case (line 244); leaf pivot-aware override in rebuild (line 184) and syncTransforms (line 356) |
| `src/editor/ui/inspectors/DoorGroupInspector.h` | VERIFIED | drawDoorGroupInspector declaration present |
| `src/editor/ui/inspectors/DoorGroupInspector.cpp` | VERIFIED | Full inspector implementation with all 9 property rows; no stubs |
| `src/editor/ui/inspectors/SceneSelectionInspector.cpp` | VERIFIED | DoorGroup case at line 159 dispatches to drawDoorGroupInspector |
| `src/editor/ui/inspectors/MeshInspector.cpp` | VERIFIED | Optional pivot row at lines 82-93 |
| `src/editor/CMakeLists.txt` | VERIFIED | DoorGroupInspector.cpp included at line 33 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `LevelDef.cpp` | `LevelDef.h` | LevelDoorGroupPlacement in LevelDef::doors vector | WIRED | `std::vector<LevelDoorGroupPlacement> doors` at LevelDef.h line 136; parser constructs LevelDoorGroupPlacement and pushes to result.doors |
| `LevelBuilder.cpp` | `GameplayPrefabs.h` | calls makePivotLeafModel for leaf transform | WIRED | Line 319: `makePivotLeafModel(group.position, group.yawDegrees, pivot, leafPlacement->scale)` |
| `LevelLoader.cpp` | `LevelBuilder.h` | calls addDoorGroup for each door placement | WIRED | Line 90: `builder.addDoorGroup(doorGroup, level)` in loop over level.doors |
| `EditorSceneDocument.cpp` | `LevelDef.h` | LevelDoorGroupPlacement in variant and toLevelDef | WIRED | Line 675: `level.doors.push_back(p)` in toLevelDef; Line 605: applyWorldTransform branch for LevelDoorGroupPlacement |
| `EditorPreviewWorld.cpp` | `GameplayPrefabs.h` | calls makePivotLeafModel for leaf transform | WIRED | Lines 184 and 356: makePivotLeafModel called in both rebuild() and syncTransforms() |
| `SceneSelectionInspector.cpp` | `DoorGroupInspector.h` | dispatches DoorGroup case to drawDoorGroupInspector | WIRED | Line 160: `drawDoorGroupInspector(std::get<LevelDoorGroupPlacement>(object->payload), ...)` |

### Data-Flow Trace (Level 4)

Data-flow trace skipped for this phase — it is a pure architecture refactoring phase with no new dynamic data sources. All data flows through the same serialization path (scene file -> LevelDef -> ECS entities) that existed before. The refactoring replaces the data type at each stage; it does not introduce new async or dynamic data paths. The pivot math helpers are pure functions (no state, no I/O).

### Behavioral Spot-Checks

| Behavior | Result | Status |
|----------|--------|--------|
| LevelSingleDoorPlacement absent from entire src/ tree | grep returns 0 matches | PASS |
| spawnSingleDoor absent from entire src/ tree | grep returns 0 matches | PASS |
| SingleDoorSpawnSpec absent from entire src/ tree | grep returns 0 matches | PASS |
| door_group keyword appears 3 times in initial_scene.scene | grep -c returns 3 | PASS |
| pivot token appears 3 times in initial_scene.scene (one per leaf) | grep -c returns 3 | PASS |
| single_door absent from initial_scene.scene | grep -c returns 0 | PASS |
| EditorDoorLeafTag absent from entire src/ tree | grep returns 0 matches | PASS |
| DoorGroupInspector.cpp in editor/CMakeLists.txt | line 33 confirmed | PASS |
| Commits ccf0d20 and 42c1e05 exist in git log | git log confirms both | PASS |

### Requirements Coverage

Phase 19 has no REQUIREMENTS.md IDs in either plan's frontmatter (`requirements: []`). This is correct — the phase is a pure architecture refactoring (data layer + editor layer internal restructuring) that is not directly tied to any user-facing requirement in the v1 requirement set. No orphaned requirements were found; no REQUIREMENTS.md entries map to Phase 19.

### Anti-Patterns Found

No anti-patterns detected in the key modified files. Specifically:

- No TODO/FIXME/HACK/PLACEHOLDER comments in LevelBuilder.cpp, DoorGroupInspector.cpp, EditorSceneDocument.cpp, or EditorPreviewWorld.cpp
- No empty/stub implementations (`return null`, `return {}`, no-op handlers)
- No hardcoded empty data arrays flowing to rendering paths

The `(void)beforeState;` in DoorGroupInspector.cpp is a legitimate unused-parameter suppression matching the pattern used in other inspectors (e.g., GroupInspector) — not a stub.

### Human Verification Required

#### 1. Click-bubble-to-parent selection in editor viewport

**Test:** Open initial_scene.scene in the level editor. In the 3D viewport, click on a door leaf mesh (not the frame, not the group label in the outliner).
**Expected:** The DoorGroup parent object becomes selected, not the individual leaf mesh.
**Why human:** The applySelectionHit code logic is verified, but the picker hit list generation and the actual entity-to-objectId mapping require a running editor to confirm end-to-end.

#### 2. DoorGroupInspector all-fields rendering

**Test:** Select a DoorGroup in the editor. Inspect the right panel — verify all 9 properties appear: Name, Position, Yaw, Open Angle, Open Duration, Interact Distance, Interact Dot Threshold, Locked (checkbox), and conditionally Locked Prompt.
**Expected:** All rows render correctly with correct value types (DragFloat3, DragFloat, SliderFloat, checkbox, text input).
**Why human:** ImGui layout and rendering correctness requires visual confirmation.

#### 3. Pivot-aware leaf position update on gizmo move

**Test:** Select a DoorGroup in the editor. Use the translate gizmo to move it. Observe the door leaf mesh in the viewport.
**Expected:** The leaf mesh moves with the group and maintains its pivot offset (does not snap to world origin or misplace relative to the hinge).
**Why human:** syncTransforms pivot recalculation path is code-verified but the visual result of `makePivotLeafModel` with the updated group transform needs confirmation.

### Gaps Summary

No gaps. All 18 observable truths are verified with direct code evidence. Both plans' artifacts exist, are substantive (no stub implementations), and are fully wired. Legacy code (LevelSingleDoorPlacement, spawnSingleDoor, SingleDoorSpawnSpec, EditorDoorLeafTag, SingleDoorInspector) is cleanly deleted. The new architecture is consistent end-to-end: scene file format, parser, serializer, hierarchy resolver, runtime builder, editor document, editor preview world, selection system, viewport interaction, command dispatch, and inspector all use the new door group types.

---

_Verified: 2026-04-07T00:30:00Z_
_Verifier: Claude (gsd-verifier)_
