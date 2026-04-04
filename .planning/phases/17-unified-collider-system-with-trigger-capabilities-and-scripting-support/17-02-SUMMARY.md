---
phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
plan: 02
subsystem: physics
tags: [jolt, physics, sensor, trigger, collider, ecs, character-controller]

# Dependency graph
requires:
  - phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
    plan: 01
    provides: ColliderComponent.h with ColliderShape/ColliderMode enums and pendingEnter/pendingExit flags

provides:
  - PhysicsSystem creates Jolt sensor bodies (mIsSensor=true) for Trigger and SolidAndTrigger ColliderComponents
  - CharacterContactListener (SensorContactListener) registered on CharacterVirtual detects sensor body overlaps
  - PhysicsSystem drains pending contacts and sets pendingEnter/pendingExit on ColliderComponent after physics step
  - BehaviorSystem reads trigger flags from ColliderComponent (not TriggerComponent)
  - TriggerSystem deleted — manual AABB/sphere overlap detection gone
  - DoorAnimationSystem and GameplayPrefabs migrated to ColliderComponent
  - pixel-roguelike builds and links cleanly

affects:
  - 17-03 (editor — builds editor authoring UI on top of ColliderComponent; may reference StaticColliderComponent still)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - SensorContactListener pattern: inner class implementing JPH::CharacterContactListener registered on CharacterVirtual for sensor body callbacks
    - Dual-body pattern: SolidAndTrigger mode creates two Jolt bodies (one solid, one sensor) with identical shape and position
    - Pending-contact drain pattern: pending vector accumulated during physics step, drained after ExtendedUpdate in same frame

key-files:
  created: []
  modified:
    - src/engine/physics/PhysicsSystem.cpp
    - src/game/behavior/BehaviorSystem.cpp
    - src/game/behavior/DoorAnimationSystem.cpp
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/game/CMakeLists.txt
    - apps/runtime/main.cpp
  deleted:
    - src/game/behavior/TriggerSystem.h
    - src/game/behavior/TriggerSystem.cpp

key-decisions:
  - "CharacterVirtual uses CharacterContactListener (not global ContactListener) — global listener never fires for sensor bodies with CharacterVirtual per Jolt D-09 research"
  - "Pending contacts drained after fixed-timestep loop (not inside ExtendedUpdate callback) to avoid modifying ECS during physics step"
  - "SolidAndTrigger creates two separate Jolt bodies (solid + sensor) — cleaner than special-casing a single body to collide AND report sensor contacts"
  - "GameplayPrefabs door leaf colliders explicitly set ColliderMode::Solid to preserve blocking behavior"
  - "TriggerComponent and StaticColliderComponent files retained for Plan 03 (editor layer still references them)"

patterns-established:
  - "Sensor detection via CharacterContactListener: always register listener on CharacterVirtual; never rely on global ContactListener for CharacterVirtual sensor overlap"
  - "BodyID hashing: use GetIndexAndSequenceNumber() as uint32_t key for unordered_map"

requirements-completed: []

# Metrics
duration: 30min
completed: 2026-04-05
---

# Phase 17 Plan 02: Jolt Sensor Body Integration Summary

**Jolt sensor bodies (mIsSensor) replace manual AABB/sphere overlap detection: CharacterContactListener on CharacterVirtual fires pendingEnter/pendingExit on ColliderComponent; TriggerSystem deleted**

## Performance

- **Duration:** ~30 min
- **Started:** 2026-04-05T23:00:00Z
- **Completed:** 2026-04-04T23:33:35Z
- **Tasks:** 2
- **Files modified:** 6 modified, 2 deleted

## Accomplishments
- Extended PhysicsSystem to create Jolt sensor bodies for Trigger/SolidAndTrigger ColliderComponents, including dual solid+sensor bodies for SolidAndTrigger mode
- Registered `SensorContactListener` (implementing `JPH::CharacterContactListener`) on CharacterVirtual to receive OnContactAdded/OnContactRemoved callbacks for sensor bodies
- Updated BehaviorSystem to read `pendingEnter`/`pendingExit` from `ColliderComponent` instead of `TriggerComponent`, with mode-guard skipping Solid-mode entities
- Deleted TriggerSystem entirely; migrated DoorAnimationSystem and GameplayPrefabs from StaticColliderComponent to ColliderComponent; cleaned up CMakeLists and main.cpp registration

## Task Commits

Each task was committed atomically:

1. **Task 1: Extend PhysicsSystem with Jolt sensor bodies and CharacterContactListener** - `d37c6a9` (feat)
2. **Task 2: Delete TriggerSystem, migrate consumers to ColliderComponent** - `7d4a62a` (feat)

**Plan metadata:** (see final commit below)

## Files Created/Modified
- `src/engine/physics/PhysicsSystem.cpp` — Replaced StaticColliderComponent with ColliderComponent; added SensorContactListener inner class; added sensorBodies/dualBodies tracking maps; creates sensor bodies with mIsSensor=true; drains pending contacts after physics step
- `src/game/behavior/BehaviorSystem.cpp` — Reads ColliderComponent instead of TriggerComponent; mode guard skips Solid entities
- `src/game/behavior/DoorAnimationSystem.cpp` — Uses ColliderComponent instead of StaticColliderComponent for door leaf collider updates
- `src/game/prefabs/GameplayPrefabs.cpp` — Emplaces ColliderComponent (mode=Solid) instead of StaticColliderComponent for door leaf physics bodies
- `src/game/CMakeLists.txt` — Removed TriggerSystem.cpp from gameplay library target
- `apps/runtime/main.cpp` — Removed TriggerSystem include and addSystem registration
- `src/game/behavior/TriggerSystem.h` — DELETED
- `src/game/behavior/TriggerSystem.cpp` — DELETED

## Decisions Made
- CharacterVirtual requires `JPH::CharacterContactListener` (not the global `JPH::ContactListener`) for sensor body overlap detection — the global listener is never called for CharacterVirtual contacts per Jolt design
- Pending sensor contacts are drained after the full physics loop (not inside the ExtendedUpdate callback) to avoid mutating ECS components during the physics step
- SolidAndTrigger mode creates two separate Jolt bodies (one with mMotionType=Static, one with mIsSensor=true) sharing the same shape and position — simpler than a hybrid body that both blocks and reports overlaps
- `TriggerComponent.h` and `StaticColliderComponent.h` are intentionally NOT deleted — Plan 03 (editor layer) still references them

## Deviations from Plan

None — plan executed exactly as written. All acceptance criteria met on first build attempt.

## Issues Encountered
None.

## Known Stubs
None — PhysicsSystem now creates sensor bodies for all Trigger/SolidAndTrigger ColliderComponents loaded from scene files. The full sensor pipeline (create body → CharacterContactListener → pendingEnter/pendingExit → BehaviorSystem dispatch) is complete end-to-end.

## Next Phase Readiness
- Plan 03 (editor): ColliderComponent is the runtime component; editor inspector/gizmo authoring on top of LevelColliderPlacement is next. StaticColliderComponent/TriggerComponent can now be cleaned up from editor code in Plan 03.
- The runtime trigger detection pipeline is complete: scene loads ColliderComponent → PhysicsSystem creates sensor body → player walks in → SensorContactListener fires → pendingEnter set → BehaviorSystem dispatches onEnter actions.

---
*Phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support*
*Completed: 2026-04-05*
