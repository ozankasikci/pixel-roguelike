# Phase 17: Unified Collider System with Trigger Capabilities and Scripting Support - Context

**Gathered:** 2026-04-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the two separate collision systems — `StaticColliderComponent` (Jolt physics bodies for solid walls/floors) and `TriggerComponent` (manual AABB/sphere overlap for behavior triggers) — with a single unified `ColliderComponent` supporting Solid, Trigger, and SolidAndTrigger modes. All colliders use Jolt Physics as the backend, with sensor bodies for triggers. Expand shape support to Box, Sphere, Cylinder, and Capsule across all modes. Delete the manual overlap TriggerSystem. Scripting support is explicitly deferred.

</domain>

<decisions>
## Implementation Decisions

### Unified ColliderComponent
- **D-01:** Single `ColliderComponent` replaces both `StaticColliderComponent` and `TriggerComponent` — one component, one system
- **D-02:** Three collider modes via `ColliderMode` enum: `Solid` (blocks movement), `Trigger` (detects overlap only), `SolidAndTrigger` (blocks movement AND fires behavior events on contact)
- **D-03:** Full replacement strategy — delete both `StaticColliderComponent` and `TriggerComponent` entirely. Migrate all scene files and code in one go

### Shape Types
- **D-04:** Unified `ColliderShape` enum with four shapes: Box, Sphere, Cylinder, Capsule
- **D-05:** All four shapes available in all three modes (any shape × any mode = valid)
- **D-06:** Capsule is new — uses Jolt's native `CapsuleShape`. Parameters: halfHeight + radius

### Jolt Physics for Triggers
- **D-07:** All triggers use Jolt sensor bodies instead of manual overlap checks. Provides broadphase acceleration, rotation support, and consistent collision across all shape types
- **D-08:** The manual TriggerSystem overlap code is deleted — PhysicsSystem manages both solid bodies and sensor bodies
- **D-09:** PhysicsSystem uses Jolt `ContactListener` to detect sensor overlap events and sets pendingEnter/pendingExit flags on `ColliderComponent` (same flag pattern BehaviorSystem already reads)

### Migration
- **D-10:** Migrate all existing `.scene` files — `collider` lines and `trigger` lines both map to the new `ColliderComponent` format
- **D-11:** Scene file parser updated to read both old format (backward compat during migration) and new unified format
- **D-12:** Editor code updated: `EditorSceneDocument`, `EditorSceneObjectKind`, inspector, outliner, gizmos all switch to `ColliderComponent`
- **D-13:** BehaviorSystem reads pendingEnter/pendingExit from `ColliderComponent` instead of `TriggerComponent` — minimal change to behavior dispatch

### Scripting
- **D-14:** Scripting support is explicitly deferred to a future phase. This phase focuses purely on collider unification

### Claude's Discretion
- Exact Jolt `ObjectLayer` and `BroadPhaseLayer` assignments for sensor bodies (may need a new SENSOR layer or reuse existing)
- Contact listener callback implementation details (OnContactAdded vs OnContactPersisted)
- Whether to keep pendingEnter/pendingExit as booleans on ColliderComponent or use a separate TriggerState sub-struct
- Scene file syntax for the unified collider format (keywords, indentation)
- Editor gizmo rendering updates for the new shape types (capsule wireframe)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Current collider system (being replaced)
- `src/game/components/StaticColliderComponent.h` — Current ColliderShape enum (Box/Cylinder), position/rotation/halfExtents fields
- `src/engine/physics/PhysicsSystem.h` — Jolt pimpl wrapper: character controller API, static body management
- `src/engine/physics/PhysicsSystem.cpp` — Full Jolt integration: layer definitions, body creation, contact filters, character controller

### Current trigger system (being replaced)
- `src/game/behavior/TriggerComponent.h` — TriggerShape enum (Box/Sphere), runtime flags (playerInside, pendingEnter, pendingExit)
- `src/game/behavior/TriggerSystem.h` — Manual overlap system interface
- `src/game/behavior/TriggerSystem.cpp` — Manual AABB/sphere overlap detection against player position

### Behavior system (consumer of trigger events)
- `src/game/behavior/BehaviorSystem.h` — Reads TriggerComponent flags, dispatches action lists
- `src/game/behavior/BehaviorSystem.cpp` — `processTriggerFlags()` reads pendingEnter/pendingExit — must update to read from ColliderComponent
- `src/game/behavior/BehaviorComponent.h` — onActivate/onEnter/onExit/onTimer action lists
- `src/game/behavior/ActionTypes.h` — All 14 action types and typed ActionParams variant

### Level data model
- `src/game/level/LevelDef.h` — TriggerPlacement, LevelBoxColliderPlacement, LevelCylinderColliderPlacement structs (being unified)
- `src/game/level/LevelDef.cpp` — Scene file parser (must handle new unified format)
- `src/game/level/LevelBuilder.h` — Entity spawning from placement data
- `src/game/level/LevelBuilder.cpp` — `addCollider()` and `addTrigger()` methods (being unified)

### Editor integration
- `src/editor/scene/EditorSceneDocument.h` — EditorSceneObjectKind has BoxCollider, CylinderCollider, Trigger kinds (being unified)
- `src/editor/scene/EditorSceneDocument.cpp` — `loadFromSceneFile()`, `toLevelDef()`, `save()`
- `src/editor/ui/EditorInspectorPanel.cpp` — Collider and trigger inspector sections
- `src/editor/ui/EditorOutlinerPanel.cpp` — Outliner rendering for collider/trigger kinds
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Wireframe rendering for colliders and triggers
- `src/editor/viewport/EditorViewportInteraction.cpp` — Gizmo interaction for collider/trigger selection

### Scene files to migrate
- `assets/scenes/initial_scene.scene` — Primary test scene with colliders and triggers
- `assets/scenes/country_house.scene` — Scene with door colliders

### Existing tests
- `tests/game/test_behavior_trigger_roundtrip.cpp` — Trigger round-trip test (must update)
- `tests/game/test_level_roundtrip.cpp` — Level round-trip test (must update)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `PhysicsSystem::Impl` — Already manages static bodies and character controllers via Jolt. Sensor body management follows the same pattern (create, track, destroy)
- `makeColliderShape()` helper in PhysicsSystem.cpp — Already creates Jolt shapes from StaticColliderComponent. Extend to support Sphere and Capsule
- `EditorSceneObject` + `EditorSceneObjectKind` — Already has BoxCollider, CylinderCollider, Trigger variants. Consolidate to a single Collider kind
- `BehaviorSystem::processTriggerFlags()` — Already reads pendingEnter/pendingExit. Just needs to read from ColliderComponent instead of TriggerComponent

### Established Patterns
- Components are POD structs (no methods, no inheritance) — new ColliderComponent follows this
- PhysicsSystem uses pimpl to hide Jolt internals — sensor body logic stays behind pimpl
- Scene file uses line-based format with indented sub-lines for behaviors
- Editor objects follow: add to kind enum → add payload → add to toLevelDef() → add outliner/inspector rendering

### Integration Points
- `PhysicsSystem::update()` — Must create sensor bodies for Trigger/SolidAndTrigger modes, register ContactListener
- `BehaviorSystem::processTriggerFlags()` — Must query ColliderComponent instead of TriggerComponent
- `LevelDef` parser — Must parse new unified collider syntax and migrate old box_collider/cylinder_collider/trigger lines
- `LevelBuilder` — Single `addCollider()` method replaces separate collider and trigger creation
- `EditorSceneDocument` — Consolidate BoxCollider/CylinderCollider/Trigger kinds into single Collider kind
- `apps/runtime/main.cpp` — Remove TriggerSystem registration (PhysicsSystem handles overlap)

</code_context>

<specifics>
## Specific Ideas

- The Source engine model continues to be the reference: brushes can be solid, trigger, or both (func_brush + trigger_multiple)
- The SolidAndTrigger mode is the key unification win — a door frame that blocks movement AND fires events when touched
- Jolt's `BodyCreationSettings::SetIsSensor(true)` is the mechanism for creating sensor bodies
- Existing BehaviorSystem flag-reading pattern (pendingEnter/pendingExit) should be preserved to minimize behavior system changes

</specifics>

<deferred>
## Deferred Ideas

- **Scripting support** — Explicitly deferred from this phase. Lua/conditional branching for behaviors can be its own phase when behavior complexity justifies it
- **Mesh colliders** — Convex hull or triangle mesh shapes. Complex to implement, not needed for current scenes
- **Dynamic colliders** — Moving physics bodies (kinematic/dynamic). Current colliders are all static
- **Collision layers/masks** — Fine-grained control over which objects collide with which. Current Jolt setup uses simple MOVING/NON_MOVING layers

</deferred>

---

*Phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support*
*Context gathered: 2026-04-05*
