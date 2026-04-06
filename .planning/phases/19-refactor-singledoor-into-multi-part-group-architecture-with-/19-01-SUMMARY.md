---
phase: 19-refactor-singledoor-into-multi-part-group-architecture-with-
plan: "01"
subsystem: game/level, game/prefabs
tags: [door-system, level-data, ecs, refactor, scene-format]
one_liner: "Replace monolithic LevelSingleDoorPlacement with LevelDoorGroupPlacement + child mesh architecture using pivot-based leaf rotation"
dependency_graph:
  requires: []
  provides:
    - LevelDoorGroupPlacement data type
    - pivot field on LevelMeshPlacement
    - door_group parser and serializer in LevelDef.cpp
    - makePivotLeafModel and computeHingeWorldPos shared helpers
    - LevelBuilder::addDoorGroup runtime spawner
    - migrated initial_scene.scene with 3 door groups
  affects:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/game/prefabs/GameplayPrefabs.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/prefabs/GameplayPrefabData.h
    - assets/scenes/initial_scene.scene
tech_stack:
  added: []
  patterns:
    - "door_group scene keyword: name x y z yaw [optional params] [node id]"
    - "pivot token on mesh lines: pivot x y z"
    - "leaf mesh identified by presence of pivot field (has_value())"
    - "frame mesh identified by absence of pivot field"
key_files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/game/prefabs/GameplayPrefabs.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/prefabs/GameplayPrefabData.h
    - assets/scenes/initial_scene.scene
decisions:
  - "Leaf mesh identified by pivot.has_value() — no separate enum or flag needed"
  - "centerOffsetFromHinge hardcoded to (0.445, 0, 0) — geometry-specific value for SM_DoorA/C/D family"
  - "Locked doors get InteractableComponent only (no DoorComponent), matching old spawnSingleDoor behavior"
  - "addDoorGroup returns entt::null if no leaf mesh found — silent skip with spdlog warn"
  - "door_group children skipped in normal LevelLoader mesh loop via doorChildNodeIds set"
metrics:
  duration_minutes: 15
  completed_date: "2026-04-06"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 9
---

# Phase 19 Plan 01: Refactor Data Layer to Door Group Architecture Summary

Replace monolithic LevelSingleDoorPlacement with LevelDoorGroupPlacement + child mesh architecture using pivot-based leaf rotation and a shared math helper to eliminate duplicated pivot math.

## What Was Built

### Task 1: Data types, parser/serializer, shared pivot helpers, scene migration

**LevelDef.h changes:**
- Added `std::optional<glm::vec3> pivot` field to `LevelMeshPlacement` for hinge-based door leaf transforms
- Added `LevelDoorGroupPlacement` struct (name, position, yawDegrees, openAngle, openDuration, interactDistance, interactDotThreshold, locked, lockedPrompt, nodeId, parentNodeId)
- Added `std::vector<LevelDoorGroupPlacement> doors` to `LevelDef`

**LevelDef.cpp changes:**
- Added `pivot` token parsing in mesh line parser (alongside `tint` and `material`)
- Added `door_group` parser block handling name, position, yaw, and optional params
- Added `pivot` serialization on mesh lines when set
- Added `door_group` serializer block
- Added `LevelNodeRef::Kind::DoorGroup` enum value
- Added DoorGroup handling in `resolveLevelHierarchy` (both `localMatrixFor` and the write-back switch)

**GameplayPrefabs.h/.cpp changes:**
- Added `makePivotLeafModel(groupWorldPos, groupYawDeg, pivot, scale) -> glm::mat4` free function
- Added `computeHingeWorldPos(groupWorldPos, groupYawDeg, pivot) -> glm::vec3` free function
- Removed `spawnSingleDoor` declaration and implementation

**GameplayPrefabData.h changes:**
- Removed `SingleDoorSpawnSpec` struct entirely

**initial_scene.scene migration:**
- Replaced 6 individual door mesh entries + 1 group node with 3 `door_group` entries
- Each door group: 1 `door_group` line + 1 frame mesh line + 1 leaf mesh line with `pivot -0.45 0.0 0.04`
- Removed old `group Group ... node node_123` node that was used as a workaround

### Task 2: LevelBuilder::addDoorGroup and LevelLoader wiring

**LevelBuilder.h/.cpp changes:**
- Added `addDoorGroup(const LevelDoorGroupPlacement& group, const LevelDef& level)` method
- Finds child meshes by `parentNodeId == group.nodeId`
- Identifies leaf (pivot present) vs frame (no pivot)
- Spawns frame as normal static mesh via `addMesh`
- Spawns leaf with pivot-aware `modelOverride` from `makePivotLeafModel`
- Attaches `DoorLeafComponent` with hinge position from `computeHingeWorldPos`
- Creates door root entity with `DoorComponent` + `InteractableComponent` (openable) or `InteractableComponent` only (locked)
- Added includes: DoorComponent.h, DoorLeafComponent.h, GameplayPrefabs.h

**LevelLoader.cpp changes:**
- Added `std::unordered_set<std::string> doorChildNodeIds` to skip door group children in normal mesh loop
- Added `for (const auto& doorGroup : level.doors)` loop calling `builder.addDoorGroup(doorGroup, level)`
- Added `#include <unordered_set>`

## Verification Results

All plan verification checks pass:
- `grep -r "LevelSingleDoorPlacement" src/game/` returns 0 matches
- `grep -r "spawnSingleDoor" src/game/` returns 0 matches
- `grep -r "SingleDoorSpawnSpec" src/game/` returns 0 matches
- `grep -c "door_group" assets/scenes/initial_scene.scene` returns 3
- `grep -c "single_door" assets/scenes/initial_scene.scene` returns 0
- `grep -c "pivot" assets/scenes/initial_scene.scene` returns 3 (one per leaf mesh)
- `makePivotLeafModel` confirmed in GameplayPrefabs.h
- `addDoorGroup` confirmed in LevelBuilder.h
- `cmake --build build --target pixel-roguelike --parallel` exits with code 0

Note: Editor code (src/editor/) still references SingleDoor types — this is expected and resolved by Plan 02.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Scene file had individual mesh entries instead of single_door**

- **Found during:** Task 1
- **Issue:** The plan assumed `initial_scene.scene` contained `single_door` keyword entries. The actual file had evolved to use individual `mesh` entries (frame + leaf) with a `group` node parent. The migration target was different but the end result is the same: the new `door_group` format.
- **Fix:** Replaced the 6 mesh entries + 1 group node with 3 `door_group` + 6 child mesh entries.
- **Files modified:** assets/scenes/initial_scene.scene

**2. [Rule 1 - Bug] InteractableComponent has no `activated` field**

- **Found during:** Task 2
- **Issue:** The plan's `addDoorGroup` code used `interact.activated = group.locked` but `InteractableComponent` has `enabled` and `busy` fields, not `activated`.
- **Fix:** Used the same pattern as the original `spawnSingleDoor`: locked doors get `InteractableComponent` only (no `DoorComponent`), which is the correct behavioral equivalent.
- **Files modified:** src/game/level/LevelBuilder.cpp

**3. [Rule 1 - Bug] DoorComponent aggregate initialization mismatch**

- **Found during:** Task 2
- **Issue:** The plan's code used `DoorComponent{leftLeaf, entt::null, interactDistance, ...}` but `DoorComponent` struct doesn't have `interactDistance`/`interactDotThreshold` fields (those live on `InteractableComponent`).
- **Fix:** Set `DoorComponent` fields individually to match the actual struct layout.
- **Files modified:** src/game/level/LevelBuilder.cpp

## Known Stubs

None — all door group functionality is fully wired. The editor integration (Plan 02) will add editor-side support for the new types.

## Self-Check: PASSED

All key files confirmed present. Both task commits (7ca515f, 995597a) confirmed in git log.
