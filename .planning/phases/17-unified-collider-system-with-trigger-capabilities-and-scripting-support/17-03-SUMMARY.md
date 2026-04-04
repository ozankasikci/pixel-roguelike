---
phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
plan: 03
subsystem: editor
tags: [editor, collider, inspector, outliner, ui, placement, ecs]

# Dependency graph
requires:
  - phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
    plan: 01
    provides: ColliderComponent, LevelColliderPlacement, unified Collider EditorSceneObjectKind
affects:
  - editor inspector (collider authoring UI)
  - editor outliner (Add Collider sub-menu)
  - editor viewport placement (unified Collider placement kind)
  - editor preview renderer (wireframe coloring by mode)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Enum encoding in state strings: meshId encodes collider shape ("box"/"cylinder"/"sphere"/"capsule") for unified EditorPlacementKind::Collider
    - Conditional inspector sections: trigger-specific widgets (Fire Once, behavior sections) shown only when ColliderMode != Solid

key-files:
  created: []
  modified:
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/ui/EditorOutlinerPanel.cpp
    - src/editor/ui/EditorAssetBrowserPanel.cpp
    - src/editor/ui/EditorPanelUtils.cpp
    - src/editor/ui/LevelEditorUi.h
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - apps/level_editor/main.cpp
    - tests/game/test_cathedral_prefabs.cpp

key-decisions:
  - "EditorPlacementKind consolidated from BoxCollider+CylinderCollider to single Collider; shape encoded in state.meshId string ('box'/'cylinder'/'sphere'/'capsule') for backward-compatible routing"
  - "Inspector Collider case now shows Shape dropdown, Mode dropdown, shape-specific params, Rotation field, Fire Once checkbox, and behavior sections — matching the LevelColliderPlacement data model"
  - "TriggerComponent.h and StaticColliderComponent.h includes removed from all editor files; ColliderComponent.h provides the unified type"

patterns-established:
  - "Collider mode gating: trigger-specific UI (Fire Once, behaviors) shown only when mode != Solid — reduces clutter for pure physics colliders"

requirements-completed: []

# Metrics
duration: 75min
completed: 2026-04-04
---

# Phase 17 Plan 03: Editor Collider UI Unification Summary

**Unified editor collider placement (Shape/Mode dropdowns, Add Collider sub-menu, stale include cleanup) completing the 3-kind (BoxCollider/CylinderCollider/Trigger) to 1-kind (Collider) editor consolidation**

## Performance

- **Duration:** ~75 min
- **Started:** 2026-04-04T22:45:00Z
- **Completed:** 2026-04-04T23:40:03Z
- **Tasks:** 2 (Task 1 pre-completed by Plan 01; Task 2 implemented here)
- **Files modified:** 9

## Accomplishments

- **Inspector**: Added Shape combo (Box/Sphere/Cylinder/Capsule) and Mode combo (Solid/Trigger/Solid+Trigger) dropdowns to the unified Collider inspector case; added Rotation field, conditional shape-specific params, Fire Once checkbox, and behavior sections that appear when mode != Solid
- **Outliner**: Replaced old "Add Trigger Zone" menu item with "Add Collider" sub-menu (Box/Sphere/Cylinder/Capsule solid shapes + Box Trigger Zone); kept top-level "Add Trigger Zone" for quick access
- **Asset Browser**: Replaced "Place Box Collider" / "Place Cylinder Collider" menu items with a "Place Collider" sub-menu offering Box/Sphere/Cylinder/Capsule options
- **LevelEditorUi.h**: Consolidated `EditorPlacementKind::BoxCollider` and `CylinderCollider` into unified `Collider` kind; shape encoded in `state.meshId`
- **EditorPanelUtils**: `beginPlacement` handles `Collider` kind by storing shape in `meshId`
- **EditorViewportInteraction**: Placement action and ghost rendering updated for unified `Collider` kind; removed stale `TriggerComponent.h` include
- **EditorScenePreviewRenderer**: Removed stale `TriggerComponent.h` and `StaticColliderComponent.h` includes
- **apps/level_editor**: Updated main.cpp Place Collider menu to sub-menu with shape options
- **test_cathedral_prefabs**: Updated door leaf assertions to use `ColliderComponent` (matching what `spawnDoorLeaf` actually emits)

## Task Commits

1. **Task 1** (pre-completed by Plan 01) — verified in place; no new commit needed
2. **Task 2: Editor panels, renderers, tests unified** — `b7dbd7f`

## Files Modified

- `src/editor/ui/EditorInspectorPanel.cpp` — Shape/Mode dropdowns, Rotation, Fire Once, behavior sections; replaced TriggerComponent.h include with ColliderComponent.h
- `src/editor/ui/EditorOutlinerPanel.cpp` — Add Collider sub-menu (Box/Sphere/Cylinder/Capsule + Trigger Zone)
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — Place Collider sub-menu replacing Box/Cylinder Collider items
- `src/editor/ui/EditorPanelUtils.cpp` — Handle Collider placement kind in beginPlacement
- `src/editor/ui/LevelEditorUi.h` — Consolidate BoxCollider+CylinderCollider → Collider in EditorPlacementKind enum
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Remove stale TriggerComponent.h and StaticColliderComponent.h includes
- `src/editor/viewport/EditorViewportInteraction.cpp` — Use unified Collider kind; remove stale TriggerComponent.h include
- `apps/level_editor/main.cpp` — Update Place Collider menu to sub-menu
- `tests/game/test_cathedral_prefabs.cpp` — Update door leaf assertions to use ColliderComponent

## Decisions Made

- `EditorPlacementKind` consolidated from 3 kinds (BoxCollider, CylinderCollider, plus Trigger handled separately) to 1 unified `Collider` kind; collider shape passed via `state.meshId` string encoding (`"box"`, `"cylinder"`, `"sphere"`, `"capsule"`) — simpler than adding a new field to EditorPlacementState
- Inspector Collider case now fully reflects the LevelColliderPlacement data model with all authoring controls
- Fire Once checkbox and behavior sections are conditionally shown only when `ColliderMode != Solid` to keep the inspector clean for physics-only colliders

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] test_cathedral_prefabs using StaticColliderComponent on door leaves**
- **Found during:** Task 2 test run
- **Issue:** `spawnDoorLeaf()` was changed by Plan 01 to use `ColliderComponent` (new), but `test_cathedral_prefabs.cpp` still called `registry.get<StaticColliderComponent>(door.leftLeaf)` — causing an assertion failure in EnTT's sparse_set because no StaticColliderComponent exists on the leaf entity
- **Fix:** Updated `test_cathedral_prefabs.cpp` to include `ColliderComponent.h` and use `registry.get<ColliderComponent>` for door leaf assertions; the assertions check the same position/halfExtents values as before
- **Files modified:** `tests/game/test_cathedral_prefabs.cpp`
- **Commit:** b7dbd7f (Task 2 commit)

**2. [Rule 3 - Blocking] apps/level_editor/main.cpp uses removed BoxCollider/CylinderCollider enum values**
- **Found during:** Task 2 build after updating LevelEditorUi.h
- **Issue:** `apps/level_editor/main.cpp` still referenced `EditorPlacementKind::BoxCollider` and `::CylinderCollider` in its menu rendering code, causing compile errors after the enum was consolidated
- **Fix:** Updated main.cpp menu items to use the "Place Collider" sub-menu with shape options, matching the pattern used in EditorAssetBrowserPanel
- **Files modified:** `apps/level_editor/main.cpp`
- **Commit:** b7dbd7f (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)

## Known Stubs

None — all collider data is fully wired. Inspector exposes all LevelColliderPlacement fields. Preview renderer colors wireframes by ColliderMode. Behavior sections in the inspector connect to the existing BehaviorDeclaration system.

## Out-of-Scope Issues Deferred

`procedural-model-viewer` target fails to build with `'game/levels/GameAssets.h' file not found` — this is a pre-existing issue unrelated to Phase 17 changes. Logged for future cleanup.

## Self-Check: PASSED

- Task 2 commit b7dbd7f exists: FOUND
- `src/editor/ui/EditorInspectorPanel.cpp` contains `ColliderMode`: FOUND
- `src/editor/ui/EditorOutlinerPanel.cpp` contains `addCollider`: FOUND
- `src/editor/render/EditorScenePreviewRenderer.cpp` uses `ColliderComponent` only: FOUND
- No `StaticColliderComponent` or `TriggerComponent` includes in `src/`: VERIFIED
- All 3 plan-specified tests pass: silos_cloister PASSED, runtime_game_session PASSED, cathedral_prefabs PASSED
- pixel-roguelike and level-editor build successfully: VERIFIED
