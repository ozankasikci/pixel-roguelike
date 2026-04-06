---
status: awaiting_human_verify
trigger: "Door/frame mesh gaps - doors don't fill wall openings"
created: 2026-04-06T00:00:00Z
updated: 2026-04-06T20:20:00Z
---

## Current Focus
<!-- OVERWRITE on each update - reflects NOW -->

hypothesis: CONFIRMED and FIXED - Wall Y scale 1.5 stretched opening to 3.15m; door/frame at 0.21 were only ~2.0m tall
test: Fixed wall_door opening to 1.4m local (2.1m at Y=1.5), changed door/frame scale to 0.22
expecting: User confirms no gaps around door frames in editor and runtime
next_action: Awaiting user verification

## Symptoms
<!-- Written during gathering, then IMMUTABLE -->

expected: Three doors (Door A west/wooden, Door B north/metal, Door C east/white) appear in initial_scene, interactable with E key, open to reveal Storage/Control Office/Utility Corridor rooms. Walls have openings where doors are.
actual: Doors completely missing — not rendered at all. Walls mispositioned/broken. Rooms exist but inaccessible.
errors: No crash — visual/geometry issues only. Doors don't spawn, wall colliders and visual geometry lack proper openings.
reproduction: Launch game, load initial_scene. Observe missing doors, broken walls, inaccessible rooms.
started: Immediately after commit 0f2184e (quick task 260406-phj)

## Eliminated
<!-- APPEND only - prevents re-investigating -->

## Evidence
<!-- APPEND only - facts discovered -->

- timestamp: 2026-04-06T00:00:00Z
  checked: debug harness socket
  found: No game process running (no socket found)
  implication: Cannot use harness to inspect live ECS state; must rely on code/file analysis

- timestamp: 2026-04-06T00:01:00Z
  checked: InitialSceneScripted.cpp, GenericFileScene.cpp, src/game/CMakeLists.txt, apps/runtime/CMakeLists.txt, apps/runtime/main.cpp
  found: InitialSceneScripted.cpp was a static library TU containing only a file-scope `static const bool kRegistered = []{ ... }()` with no externally referenced symbols. The Apple linker dead-strips such object files from static libraries. The registration callback was never called, so no doors were spawned.
  implication: Root cause confirmed. Wall collider and visual geometry in the scene file are correctly segmented with openings. Only the door spawning was broken.

- timestamp: 2026-04-06T00:02:00Z
  checked: assets/packs/QuestDoorsPack/Models/ — mesh files SM_DoorA, SM_DoorC, SM_DoorD, SM_FrameA, SM_FrameC, SM_FrameD all present; assets/materials/ — qdp_door_a, qdp_door_c, qdp_door_d materials all present
  found: All required assets exist; no asset-side issue
  implication: Once linker issue is fixed, doors should spawn and render correctly

- timestamp: 2026-04-06T00:03:00Z
  checked: Build output after fix
  found: Build succeeds cleanly; InitialSceneScripted.cpp.o and main.cpp.o both recompiled
  implication: Fix applied correctly with no compiler errors

- timestamp: 2026-04-06T00:30:00Z
  checked: EditorSceneDocument.h/cpp, EditorPreviewWorld.cpp, EditorSelectionSystem.cpp, EditorViewportInteraction.cpp, EditorCommand.cpp, SceneSelectionInspector.cpp, EditorOutlinerPanel.cpp
  found: EditorSceneObjectKind enum has no SingleDoor value; EditorSceneObjectPayload variant does not include LevelSingleDoorPlacement; loadFromSceneFile() does not iterate level.doors; toLevelDef() does not handle doors; EditorPreviewWorld::rebuild() has no SingleDoor case; applyWorldTransform/localTransformMatrix/applyGizmoToSelectedObject do not handle SingleDoor; no SingleDoor inspector exists.
  implication: Full editor support for SingleDoor must be added across all subsystems.

- timestamp: 2026-04-06T00:45:00Z
  checked: Build and runtime verification after implementing SingleDoor support across 11 files
  found: Build succeeded with zero errors. Editor launched with initial_scene.scene, loaded SM_DoorA, SM_FrameA, SM_DoorC, SM_FrameC, SM_DoorD, SM_FrameD meshes and all associated door textures. No crashes or warnings related to door handling.
  implication: SingleDoor support is correctly integrated. Door entities are created in the document, rendered in the preview world, and available for selection/inspection.

- timestamp: 2026-04-06T20:14:00Z
  checked: SM_FrameA/C/D and SM_DoorA/C/D raw AABBs via test_questdoor_scale
  found: FrameA raw 4.7672x9.5702, at 0.21 = 1.001m x 2.010m. FrameC/D raw 4.9783x9.6843. DoorA raw 4.0556x9.2150. prison_wall_door opening 0.9m x 2.1m local, scaled to 0.9m x 3.15m at Y=1.5.
  implication: Frame at 0.21 is 2.01m tall but world-space opening is 3.15m = 1.14m gap above. Root cause confirmed.

- timestamp: 2026-04-06T20:20:00Z
  checked: Scale calculations for corrected geometry
  found: Local opening height should be 1.4m (2.1/1.5). Scale 0.22 gives FrameA height 2.105m, DoorA height 2.027m, DoorA width 0.892m. Frame width 1.049m covers 0.9m opening with 75mm overlap per side.
  implication: Scale 0.22 with 1.4m local opening produces correct world-space fit.

- timestamp: 2026-04-06T20:25:00Z
  checked: Build after applying all three file changes
  found: Both pixel-roguelike and level-editor build with zero errors. ProceduralGameAssets, GameplayPrefabs, and EditorPreviewWorld all recompiled and linked successfully.
  implication: Fix is structurally sound. Needs visual verification.

## Resolution
<!-- OVERWRITE as understanding evolves -->

root_cause: (Phase 3 - gap fix) prison_wall_door opening was 2.1m tall in local space, but scene places wall_door panels at Y scale 1.5, stretching the opening to 3.15m in world space. Door/frame meshes at scale 0.21 produced only ~2.0m height, leaving a ~1.14m gap above and proportional side gaps.
fix: (1) Redesigned createPrisonWallDoor() opening from 2.1m to 1.4m local height so at Y=1.5 it becomes 2.1m world space. (2) Changed door/frame mesh scale from 0.21 to 0.22 for better fit (frame height 2.105m matches 2.1m opening). (3) Updated collider half-extents and center offset to match new scale. Changes applied to both runtime (GameplayPrefabs.cpp) and editor (EditorPreviewWorld.cpp).
verification: Build succeeded (zero errors) for both pixel-roguelike and level-editor. Awaiting user visual confirmation.
files_changed:
  - src/game/levels/ProceduralGameAssets.cpp
  - src/game/prefabs/GameplayPrefabs.cpp
  - src/editor/scene/EditorPreviewWorld.cpp
