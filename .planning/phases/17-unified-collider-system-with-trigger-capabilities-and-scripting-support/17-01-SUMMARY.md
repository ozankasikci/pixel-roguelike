---
phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
plan: 01
subsystem: game-data
tags: [collider, physics, level-format, ecs, serialization, migration]

# Dependency graph
requires:
  - phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity
    provides: trigger/behavior data structs (TriggerPlacement, LevelBoxColliderPlacement) that this plan consolidates
provides:
  - ColliderComponent.h with ColliderShape (Box/Sphere/Cylinder/Capsule) and ColliderMode (Solid/Trigger/SolidAndTrigger)
  - LevelColliderPlacement unified struct replacing 3 separate placement types
  - Backward-compatible parser: reads old collider_box/collider_cylinder/trigger_box/trigger_sphere AND new collider <shape> <mode> format
  - Serializer emitting only new unified format
  - Single LevelBuilder.addCollider() method
  - 6 scene files fully migrated to new format
affects:
  - 17-02 (physics/runtime — builds ColliderComponent queries on top of this)
  - 17-03 (editor — uses LevelColliderPlacement for inspector/gizmo authoring)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Unified placement struct pattern: single struct with discriminating enums replaces N type-specific structs
    - Backward-compatible format migration: parser reads old+new, serializer writes only new
    - StaticColliderShape rename: legacy enum renamed to avoid redeclaration when new same-named enum with underlying type is added

key-files:
  created:
    - src/game/components/ColliderComponent.h
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/game/components/StaticColliderComponent.h
    - src/game/levels/cathedral/CathedralPrefabs.cpp
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/engine/physics/PhysicsSystem.cpp
    - src/editor/scene/EditorSceneDocument.h
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/scene/EditorSelectionSystem.cpp
    - src/editor/core/EditorCommand.cpp
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/ui/EditorOutlinerPanel.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - assets/scenes/cathedral.scene
    - assets/scenes/country_house.scene
    - assets/scenes/initial_scene.scene
    - assets/scenes/institutional_room.scene
    - assets/scenes/silos_cloister.scene
    - assets/scenes/warden_office.scene
    - tests/game/test_level_def.cpp
    - tests/game/test_level_roundtrip.cpp
    - tests/game/test_behavior_trigger_roundtrip.cpp
    - tests/game/test_silos_cloister_level.cpp
    - tests/game/test_runtime_game_session.cpp
    - tests/editor/test_editor_selection.cpp

key-decisions:
  - "ColliderShape and ColliderMode use uint8_t underlying type for memory efficiency in ECS component layout"
  - "StaticColliderComponent retained unchanged (runtime physics uses it); renamed its ColliderShape to StaticColliderShape to resolve redeclaration conflict"
  - "Editor consolidated from 3 object kinds (BoxCollider, CylinderCollider, Trigger) to 1 (Collider) — simplifies outliner, inspector, and selection priority logic"
  - "Parser backward-compat: reads old collider_box/collider_cylinder/trigger_box/trigger_sphere keywords AND new unified format; serializer emits only new format"
  - "EditorScenePreviewRenderer updated to iterate ColliderComponent (new) instead of StaticColliderComponent (old) for solid collider visualization in editor viewport"

patterns-established:
  - "Unified placement struct: single LevelColliderPlacement with shape+mode enums replaces multiple type-specific structs"
  - "StaticColliderShape rename: when introducing a same-named enum with a different underlying type, rename the old one to avoid redeclaration errors"

requirements-completed: []

# Metrics
duration: 180min
completed: 2026-04-05
---

# Phase 17 Plan 01: Unified Collider Data Model Summary

**Unified ColliderShape/ColliderMode enums and LevelColliderPlacement replace 3 separate box/cylinder/trigger placement structs, with backward-compatible parser and 6 scene files migrated to new format**

## Performance

- **Duration:** ~180 min (across 2 sessions)
- **Started:** 2026-04-04T21:00:00Z
- **Completed:** 2026-04-04T23:25:31Z
- **Tasks:** 2
- **Files modified:** 30

## Accomplishments
- Created `ColliderComponent.h` with `ColliderShape` (Box/Sphere/Cylinder/Capsule) and `ColliderMode` (Solid/Trigger/SolidAndTrigger) — the unified ECS component for Plan 02's physics system
- Replaced `LevelBoxColliderPlacement`, `LevelCylinderColliderPlacement`, and `TriggerPlacement` with single `LevelColliderPlacement`; parser handles old+new formats, serializer emits only new `collider <shape> <mode>` format
- Migrated all 6 scene files (96 lines) from `collider_box`/`collider_cylinder` to `collider box solid`/`collider cylinder solid`; all round-trip tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Unified collider data model** - `f5d087a` (feat)
2. **Task 2: Scene file migration and roundtrip tests** - `409ef03` (feat)

**Plan metadata:** (see final commit below)

## Files Created/Modified
- `src/game/components/ColliderComponent.h` — New unified ECS component with ColliderShape/ColliderMode enums and trigger runtime flags
- `src/game/level/LevelDef.h` — LevelColliderPlacement struct; removed 3 old placement types; LevelDef.colliders vector
- `src/game/level/LevelDef.cpp` — Backward-compatible parser; new unified serializer output
- `src/game/level/LevelBuilder.h/cpp` — Single addCollider() replacing addBoxCollider/addCylinderCollider/addTrigger
- `src/game/components/StaticColliderComponent.h` — Renamed ColliderShape to StaticColliderShape to avoid redeclaration conflict
- `src/engine/physics/PhysicsSystem.cpp` — Updated to use StaticColliderShape::Box/Cylinder
- `src/game/prefabs/GameplayPrefabs.cpp` — Updated to use StaticColliderShape::Box
- `src/game/levels/cathedral/CathedralPrefabs.cpp` — Updated spawnCathedralDaisStep to use addCollider()
- `src/editor/scene/EditorSceneDocument.h/cpp` — Consolidated BoxCollider/CylinderCollider/Trigger kinds to single Collider kind
- `src/editor/scene/EditorPreviewWorld.cpp` — Iterates ColliderComponent (not StaticColliderComponent) for bounds/update
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Solid collider viz uses ColliderComponent; trigger viz uses LevelColliderPlacement
- `src/editor/scene/EditorSelectionSystem.cpp` — Collider selection priority = 4; Cylinder/Capsule → Cylinder handle shape
- `src/editor/core/EditorCommand.cpp` — makeLevelDefFromState uses Collider kind
- `src/editor/ui/EditorInspectorPanel.cpp` — Single Collider inspector case; stats show unified colliders count
- `src/editor/ui/EditorOutlinerPanel.cpp` — Add Trigger Zone creates LevelColliderPlacement{mode=Trigger}
- `src/editor/viewport/EditorViewportInteraction.cpp` — BoxCollider/CylinderCollider placement creates LevelColliderPlacement
- `assets/scenes/*.scene` — All 6 scene files migrated to new format
- `tests/game/test_level_def.cpp` — Uses data.colliders filtered by ColliderShape
- `tests/game/test_level_roundtrip.cpp` — Tests Box/Cylinder/Sphere/Capsule shapes and Solid/Trigger/SolidAndTrigger modes
- `tests/game/test_behavior_trigger_roundtrip.cpp` — Tests collider box/sphere trigger round-trip with behaviors
- `tests/game/test_silos_cloister_level.cpp` — Uses data.colliders with ColliderShape filters
- `tests/game/test_runtime_game_session.cpp` — Updated to use StaticColliderShape::Box
- `tests/editor/test_editor_selection.cpp` — Uses EditorSceneObjectKind::Collider

## Decisions Made
- `ColliderShape` and `ColliderMode` use `uint8_t` underlying type for cache efficiency in ECS component arrays
- `StaticColliderComponent` is intentionally NOT replaced — it is the runtime physics ECS component used by `PhysicsSystem.cpp` and `DoorAnimationSystem.cpp`; Plan 02 will bridge `ColliderComponent` → Jolt physics
- Renamed `StaticColliderComponent.h`'s `ColliderShape` to `StaticColliderShape` to avoid C++ redeclaration error (same name, different underlying type between old and new enum)
- Editor: 3 `EditorSceneObjectKind` variants (BoxCollider, CylinderCollider, Trigger) consolidated into single Collider variant — simplifies selection, inspector, and command code
- Parser backward-compatibility: old scene files still load correctly through backward-compat handlers in `LevelDef.cpp`

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Editor cascade compile failures**
- **Found during:** Task 1 (after removing LevelBoxColliderPlacement/LevelCylinderColliderPlacement/TriggerPlacement)
- **Issue:** Plan listed only game-layer files, but removing types caused cascade compile failures across 9 editor files that used `LevelBoxColliderPlacement`, `LevelCylinderColliderPlacement`, `TriggerPlacement`, and the 3 separate `EditorSceneObjectKind` variants
- **Fix:** Updated all editor files (EditorSceneDocument, EditorPreviewWorld, EditorSelectionSystem, EditorCommand, EditorScenePreviewRenderer, EditorInspectorPanel, EditorOutlinerPanel, EditorViewportInteraction) to use unified Collider kind and LevelColliderPlacement
- **Files modified:** 8 editor files
- **Verification:** level-editor executable builds cleanly; test_editor_selection passes
- **Committed in:** f5d087a (Task 1 commit)

**2. [Rule 3 - Blocking] StaticColliderShape enum redeclaration conflict**
- **Found during:** Task 1 (after ColliderComponent.h was added)
- **Issue:** `StaticColliderComponent.h` declares `enum class ColliderShape { Box, Cylinder }` (no underlying type), conflicting with `ColliderComponent.h`'s `enum class ColliderShape : uint8_t { Box, Sphere, Cylinder, Capsule }`; C++ prohibits redeclaration with different underlying type
- **Fix:** Renamed `StaticColliderComponent.h`'s enum to `StaticColliderShape`; updated `PhysicsSystem.cpp`, `GameplayPrefabs.cpp`, `test_runtime_game_session.cpp` to use `StaticColliderShape::Box/Cylinder`
- **Files modified:** `StaticColliderComponent.h`, `PhysicsSystem.cpp`, `GameplayPrefabs.cpp`, `test_runtime_game_session.cpp`
- **Verification:** All affected files compile cleanly
- **Committed in:** f5d087a (Task 1 commit)

**3. [Rule 3 - Blocking] CathedralPrefabs.cpp uses removed addBoxCollider method**
- **Found during:** Task 1 build
- **Issue:** `CathedralPrefabs.cpp:38` calls `builder.addBoxCollider(center, scale * 0.5f)` which was removed from LevelBuilder
- **Fix:** Updated to construct `LevelColliderPlacement` and call `builder.addCollider(cp)`
- **Files modified:** `src/game/levels/cathedral/CathedralPrefabs.cpp`
- **Verification:** gameplay library builds cleanly
- **Committed in:** f5d087a (Task 1 commit)

**4. [Rule 1 - Bug] EditorPreviewWorld iterating wrong component type for bounds**
- **Found during:** Task 1 editor build
- **Issue:** Previous edit changed EditorPreviewWorld to include `ColliderComponent.h` and use `ColliderComponent`, but the `colliderBounds()` function still took `StaticColliderComponent&` and the view iterated `StaticColliderComponent`; editor entities now have `ColliderComponent` (not `StaticColliderComponent`), so bounds would always be empty
- **Fix:** Updated `colliderBounds()` to take `ColliderComponent&` and view to iterate `ColliderComponent`; updated shape comparison to use `ColliderShape::Box` (new enum)
- **Files modified:** `src/editor/scene/EditorPreviewWorld.cpp`
- **Verification:** test_editor_selection passes
- **Committed in:** f5d087a (Task 1 commit)

**5. [Rule 1 - Bug] test_behavior_trigger_roundtrip false-positive assertion failure**
- **Found during:** Task 2 test run
- **Issue:** Test nodeId `trigger_box_01` contains the substring `trigger_box`, causing the assertion `serialized.find("trigger_box") == std::string::npos` to fail even though the serializer correctly outputs `collider box trigger`
- **Fix:** Renamed test nodeIds from `trigger_box_01`/`trigger_sphere_01` to `collider_trig_01`/`collider_trig_02`; updated the corresponding assertion checks
- **Files modified:** `tests/game/test_behavior_trigger_roundtrip.cpp`
- **Verification:** test_behavior_trigger_roundtrip passes
- **Committed in:** 409ef03 (Task 2 commit)

---

**Total deviations:** 5 auto-fixed (3 blocking, 1 bug, 1 bug)
**Impact on plan:** All auto-fixes were necessary for compilation and correctness. No feature scope creep — all changes are direct consequences of the data model consolidation.

## Issues Encountered
- Build infrastructure: `ar` archiving of assimp's `zip.c.o` reported "Operation not permitted" on macOS (SIP-related). Resolved by manually compiling the missing object file; subsequent builds use cached compiled output. Pre-existing issue unrelated to code changes.

## Known Stubs
None — all collider data is fully wired. ColliderComponent fields are populated from LevelColliderPlacement and stored in the ECS. Plan 02 will wire ColliderComponent → Jolt physics; Plan 03 will wire editor inspector to expose all ColliderComponent fields.

## Next Phase Readiness
- Plan 02 (physics/runtime): `ColliderComponent` is ready with all shape/mode fields; Plan 02 creates a `ColliderSystem` that queries `ColliderComponent` and creates Jolt bodies (replaces the StaticColliderComponent → Jolt path in PhysicsSystem.cpp)
- Plan 03 (editor): `LevelColliderPlacement` and unified `Collider` EditorSceneObjectKind are in place; Plan 03 adds shape-aware inspector UI and resize gizmos for non-box shapes

---
*Phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support*
*Completed: 2026-04-05*
