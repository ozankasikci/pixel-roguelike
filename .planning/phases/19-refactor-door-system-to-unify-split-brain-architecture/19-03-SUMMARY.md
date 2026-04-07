---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
plan: "03"
subsystem: prefab-system
tags: [cleanup, dead-code, door-system, content-registry]
dependency_graph:
  requires: [19-01]
  provides: [clean-prefab-system-checkpoint-only]
  affects: [ContentRegistry, GameplayPrefabs, PrefabInspector, test_cathedral_prefabs, test_content_registry]
tech_stack:
  added: []
  patterns: [delete-dead-spawn-path]
key_files:
  created: []
  modified:
    - src/game/prefabs/GameplayPrefabData.h
    - src/game/prefabs/GameplayPrefabs.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/content/ContentRegistry.h
    - src/game/content/ContentRegistry.cpp
    - src/editor/ui/inspectors/PrefabInspector.cpp
    - tests/game/test_content_registry.cpp
    - tests/game/test_cathedral_prefabs.cpp
    - tests/game/CMakeLists.txt
  deleted:
    - assets/prefabs/gameplay/double_door.prefab
decisions:
  - "Removed DoubleDoor spawn path entirely; all doors now defined via .scene door_group entries"
  - "makePivotLeafModel preserved as shared pivot math used by DoorAnimationSystem"
  - "PrefabInspector updated as Rule 3 auto-fix (compile error caused by struct deletion)"
  - "test_cathedral_prefabs trimmed to checkpoint-only as Rule 3 auto-fix"
metrics:
  duration_seconds: 451
  completed_date: "2026-04-07"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 9
  files_deleted: 1
---

# Phase 19 Plan 03: Delete Prefab-Based Door Spawn Path Summary

**One-liner:** Deleted DoubleDoor prefab spawn path (DoubleDoorSpawnSpec, spawnDoubleDoor, computeHingeWorldPos, double_door.prefab) leaving Checkpoint-only prefab system with makePivotLeafModel preserved.

## What Was Done

Eliminated the second door spawn path that caused split-brain architecture. All doors are now defined exclusively via `.scene` file `door_group` entries and handled by `LevelBuilder` + `DoorAnimationSystem`. The prefab system now only manages checkpoints.

**Files removed/deleted:**
- `DoubleDoorSpawnSpec` struct from `GameplayPrefabData.h`
- `DoubleDoor` from `GameplayPrefabType` enum
- `doubleDoor` field from `GameplayPrefabInstance`
- `spawnDoubleDoor` (both overloads), `spawnDoorLeaf`, `computeHingeWorldPos` from `GameplayPrefabs.cpp`
- `DoubleDoor` from `GameplayArchetypeKind` enum
- `doubleDoor` field from `GameplayArchetypeDefinition`
- All `double_door` parser branches and serializer cases in `ContentRegistry.cpp`
- `double_door.prefab` load call from `ContentRegistry::loadDefaults()`
- `assets/prefabs/gameplay/double_door.prefab` asset file

**Preserved:**
- `makePivotLeafModel()` — shared pivot math used by `DoorAnimationSystem`, `LevelBuilder`, and editor preview

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] PrefabInspector.cpp referenced deleted DoubleDoor types**
- **Found during:** Task 2 build
- **Issue:** `PrefabInspector.cpp` had `GameplayArchetypeKind::DoubleDoor` and `prefab.doubleDoor.*` references causing compile error
- **Fix:** Removed DoubleDoor combo option and entire DoubleDoor inspector case from `renderPrefabDraftFields`
- **Files modified:** `src/editor/ui/inspectors/PrefabInspector.cpp`
- **Commit:** 09166c2

**2. [Rule 3 - Blocking] test_content_registry.cpp referenced deleted DoubleDoor types**
- **Found during:** Task 2 build
- **Issue:** Door archetype load, `DoubleDoor` type assertions, and `doubleDoor.*` field accesses caused compile error
- **Fix:** Removed door archetype test block (lines 47–70); removed `DOOR_ARCHETYPE_FILE` define from `tests/game/CMakeLists.txt`
- **Files modified:** `tests/game/test_content_registry.cpp`, `tests/game/CMakeLists.txt`
- **Commit:** 09166c2

**3. [Rule 3 - Blocking] test_cathedral_prefabs.cpp referenced deleted spawnDoubleDoor and DoubleDoorSpawnSpec**
- **Found during:** Task 2 build
- **Issue:** Entire door spawn section used `DoubleDoorSpawnSpec`, `spawnDoubleDoor`, `DoorConfigComponent`, `DoorLeafComponent` — all pointing to deleted code
- **Fix:** Removed door test section (lines 53–111); kept checkpoint spawn test which remains valid
- **Files modified:** `tests/game/test_cathedral_prefabs.cpp`
- **Commit:** 09166c2

## Verification Results

- `grep -rn "DoubleDoor|double_door|computeHingeWorldPos|spawnDoubleDoor" src/game/prefabs/ src/game/content/` → 0 matches (PASS)
- `test ! -f assets/prefabs/gameplay/double_door.prefab` → PASS
- `grep -c "makePivotLeafModel" src/game/prefabs/GameplayPrefabs.cpp` → 1 (PASS)
- `cmake --build build` → 100% success, all targets built
- `ctest --output-on-failure` → 30/30 tests passed

## Known Stubs

None.

## Threat Flags

None — no new network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

- bb32e6f: Task 1 commit exists ✓
- 09166c2: Task 2 commit exists ✓
- `src/game/prefabs/GameplayPrefabData.h` exists and contains only Checkpoint ✓
- `assets/prefabs/gameplay/double_door.prefab` does not exist ✓
- All 30 tests pass ✓
