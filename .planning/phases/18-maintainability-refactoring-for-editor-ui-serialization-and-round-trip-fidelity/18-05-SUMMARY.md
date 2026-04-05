---
phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
plan: "05"
subsystem: editor-inspector-ui, test-fixtures
tags: [gap-closure, inspector, test-fixture, refactor, DRY]
dependency_graph:
  requires: [18-01, 18-02, 18-03, 18-04]
  provides: [unified-light-test-fixture, shared-transform-helpers]
  affects: [tests/game/test_level_lighting, all-7-inspector-files]
tech_stack:
  added: []
  patterns: [shared-helper-extraction, DRY-transform-editing]
key_files:
  created: []
  modified:
    - tests/data/light_records.scene
    - src/editor/ui/inspectors/InspectorUtils.h
    - src/editor/ui/inspectors/InspectorUtils.cpp
    - src/editor/ui/inspectors/MeshInspector.cpp
    - src/editor/ui/inspectors/LightInspector.cpp
    - src/editor/ui/inspectors/ColliderInspector.cpp
    - src/editor/ui/inspectors/GroupInspector.cpp
    - src/editor/ui/inspectors/ArchetypeInspector.cpp
    - src/editor/ui/inspectors/ReflectionProbeInspector.cpp
    - src/editor/ui/inspectors/PlayerSpawnInspector.cpp
decisions:
  - "drawPositionSection added alongside drawTransformSection/WithScale to handle inspectors that only edit position (Light, Archetype, ReflectionProbe, PlayerSpawn)"
  - "itemBefore variable re-declared with auto after transform blocks where downstream code still needed it (MeshInspector tint, ColliderInspector shape params)"
  - "Pre-existing level-editor linker error (assetInspectorSession undefined) confirmed to predate this plan; deferred"
metrics:
  duration: "~20 minutes"
  completed: "2026-04-05T03:11:06Z"
  tasks_completed: 2
  files_modified: 10
---

# Phase 18 Plan 05: Gap Closure — Light Fixture Migration and Transform Helper Extraction Summary

Gap closure plan addressing two verification failures from Phase 18: unified light format not applied to the test fixture (blocker), and inline transform editing boilerplate still present across all inspector files.

## What Was Built

**Gap 1 (Blocker):** Migrated `tests/data/light_records.scene` from the legacy `light`/`spot_light`/`dir_light` format to the unified `light point`/`light spot`/`light directional` syntax. The `test_level_lighting` test now passes (exit 0).

**Gap 3:** Extracted three shared transform editing helpers into `InspectorUtils.h/.cpp`:
- `drawPositionSection` — single position row (used by Light, Archetype, ReflectionProbe, PlayerSpawn)
- `drawTransformSection` — position + rotation rows (used by Collider)
- `drawTransformSectionWithScale` — position + scale + rotation rows with `glm::max` clamp (used by Mesh, Group)

All 7 inspector files now call shared helpers instead of duplicating the `captureState` / `editVec3` / `trackLastItemCommand` boilerplate for transform fields.

## Tasks

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Migrate test fixture and add drawTransformSection utilities | 15089ed | tests/data/light_records.scene, InspectorUtils.h, InspectorUtils.cpp |
| 2 | Replace inline transform editing in all 7 inspector files | 27ba20b | 7 inspector .cpp files |

## Verification Results

- `test_level_lighting` exits 0
- `cmake --build build-test --target editor` succeeds (libeditor.a links cleanly)
- `cmake --build build-test --target pixel-roguelike` succeeds
- 3 helper declarations in InspectorUtils.h
- 7 inspector files use shared helpers (confirmed by grep)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] itemBefore variable undeclared after transform block removal**
- **Found during:** Task 2 build
- **Issue:** After replacing inline position/scale/rotation blocks with `drawTransformSectionWithScale`, `MeshInspector.cpp` and `ColliderInspector.cpp` still used `itemBefore` for downstream fields (tint, shape params), but the variable was no longer declared in scope
- **Fix:** Added `auto itemBefore = document.captureState();` re-declaration immediately after the shared helper call, before the downstream fields that need it
- **Files modified:** MeshInspector.cpp, ColliderInspector.cpp
- **Commit:** 27ba20b

### Scope Notes

- `drawPositionSection` was added to InspectorUtils (beyond the plan's original two-function scope) because 4 of the 7 inspectors only edit position — `drawTransformSection` would have incorrectly rendered a rotation row for them.
- Pre-existing linker error in `level-editor` executable (`assetInspectorSession` undefined symbol) was confirmed to predate this plan; deferred to `deferred-items.md`.

## Known Stubs

None. All changes are implementation completions, not stubs.

## Self-Check: PASSED

All key files confirmed present. Both task commits confirmed in git log.
