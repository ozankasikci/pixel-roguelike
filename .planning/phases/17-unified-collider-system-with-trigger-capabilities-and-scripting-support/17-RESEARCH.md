# Phase 17: Unified Collider System with Trigger Capabilities and Scripting Support - Research

**Researched:** 2026-04-05
**Domain:** Jolt Physics sensor bodies, ECS unified collider design, scene file migration
**Confidence:** HIGH

## Summary

Phase 17 replaces two separate collision systems — `StaticColliderComponent` (Jolt rigid bodies) and `TriggerComponent` (manual AABB/sphere overlap) — with a single `ColliderComponent` supporting Solid, Trigger, and SolidAndTrigger modes. All shapes use Jolt Physics as the backend. Triggers become Jolt sensor bodies detected via `CharacterContactListener` on the existing `CharacterVirtual`.

The current codebase has no `.scene` files with `trigger_box`/`trigger_sphere` lines — all trigger content is author-created in the editor and not yet committed to any scene file. All scene files use only `collider_box` and `collider_cylinder` syntax. This simplifies migration: the parser update adds a new `collider` keyword with a `mode` token, while `collider_box`/`collider_cylinder` remain supported for existing files.

The critical architectural finding is that `CharacterVirtual` does NOT fire the global `JPH::ContactListener` when it contacts sensor bodies. It fires its own `CharacterContactListener::OnContactAdded/OnContactPersisted/OnContactRemoved` with an `mIsSensorB = true` flag on the contact. The PhysicsSystem's sensor overlap tracking must therefore be done via `CharacterContactListener` — not via the global `ContactListener`.

**Primary recommendation:** Implement sensor detection through a `CharacterContactListener` subclass registered on the existing `CharacterVirtual`. Map `JPH::BodyID` → `entt::entity` for sensor lookup. Set `pendingEnter`/`pendingExit` on `ColliderComponent` post-update (after `ExtendedUpdate()` returns). This preserves the existing BehaviorSystem flag-consumption pattern with zero changes to BehaviorSystem.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Single `ColliderComponent` replaces both `StaticColliderComponent` and `TriggerComponent` — one component, one system
- **D-02:** Three collider modes via `ColliderMode` enum: `Solid` (blocks movement), `Trigger` (detects overlap only), `SolidAndTrigger` (blocks movement AND fires behavior events on contact)
- **D-03:** Full replacement strategy — delete both `StaticColliderComponent` and `TriggerComponent` entirely. Migrate all scene files and code in one go
- **D-04:** Unified `ColliderShape` enum with four shapes: Box, Sphere, Cylinder, Capsule
- **D-05:** All four shapes available in all three modes (any shape × any mode = valid)
- **D-06:** Capsule is new — uses Jolt's native `CapsuleShape`. Parameters: halfHeight + radius
- **D-07:** All triggers use Jolt sensor bodies instead of manual overlap checks. Provides broadphase acceleration, rotation support, and consistent collision across all shape types
- **D-08:** The manual TriggerSystem overlap code is deleted — PhysicsSystem manages both solid bodies and sensor bodies
- **D-09:** PhysicsSystem uses Jolt `ContactListener` to detect sensor overlap events and sets pendingEnter/pendingExit flags on `ColliderComponent` (same flag pattern BehaviorSystem already reads)
- **D-10:** Migrate all existing `.scene` files — `collider` lines and `trigger` lines both map to the new `ColliderComponent` format
- **D-11:** Scene file parser updated to read both old format (backward compat during migration) and new unified format
- **D-12:** Editor code updated: `EditorSceneDocument`, `EditorSceneObjectKind`, inspector, outliner, gizmos all switch to `ColliderComponent`
- **D-13:** BehaviorSystem reads pendingEnter/pendingExit from `ColliderComponent` instead of `TriggerComponent` — minimal change to behavior dispatch
- **D-14:** Scripting support is explicitly deferred to a future phase. This phase focuses purely on collider unification

### Claude's Discretion
- Exact Jolt `ObjectLayer` and `BroadPhaseLayer` assignments for sensor bodies (may need a new SENSOR layer or reuse existing)
- Contact listener callback implementation details (OnContactAdded vs OnContactPersisted)
- Whether to keep pendingEnter/pendingExit as booleans on ColliderComponent or use a separate TriggerState sub-struct
- Scene file syntax for the unified collider format (keywords, indentation)
- Editor gizmo rendering updates for the new shape types (capsule wireframe)

### Deferred Ideas (OUT OF SCOPE)
- **Scripting support** — Explicitly deferred from this phase. Lua/conditional branching for behaviors can be its own phase when behavior complexity justifies it
- **Mesh colliders** — Convex hull or triangle mesh shapes. Complex to implement, not needed for current scenes
- **Dynamic colliders** — Moving physics bodies (kinematic/dynamic). Current colliders are all static
- **Collision layers/masks** — Fine-grained control over which objects collide with which. Current Jolt setup uses simple MOVING/NON_MOVING layers
</user_constraints>

---

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Relevance |
|---------|---------|---------|-----------|
| Jolt Physics | v5.4.0 | Rigid body simulation, sensor bodies, CapsuleShape | `BodyCreationSettings::mIsSensor`, `CharacterContactListener` |
| EnTT | v3.16.0 | ECS registry | `ColliderComponent` as POD, views, entity ID mapping |
| GLM | 1.0.3 | Math | Shape parameter storage (halfExtents, position, rotation) |
| Dear ImGui | v1.92.6 | Editor inspector UI | Collider inspector panel |

### New Jolt Headers Required
```cpp
// Already included in PhysicsSystem.cpp:
#include <Jolt/Physics/Collision/Shape/BoxShape.h>       // existing
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>  // existing
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>   // existing (used for CharacterVirtual)

// New includes needed:
#include <Jolt/Physics/Collision/Shape/SphereShape.h>    // for Sphere colliders
#include <Jolt/Physics/Collision/ContactListener.h>      // already pulled via PhysicsSystem.h
#include <Jolt/Physics/Character/CharacterVirtual.h>     // already included
```

### Installation
No new libraries. All required Jolt shapes are already vendored via FetchContent.

---

## Architecture Patterns

### Recommended ColliderComponent Structure

```cpp
// src/game/components/ColliderComponent.h
#pragma once
#include <glm/glm.hpp>
#include <cstdint>

enum class ColliderShape : uint8_t { Box, Sphere, Cylinder, Capsule };
enum class ColliderMode  : uint8_t { Solid, Trigger, SolidAndTrigger };

struct ColliderComponent {
    ColliderShape shape = ColliderShape::Box;
    ColliderMode  mode  = ColliderMode::Solid;

    glm::vec3 position{0.0f};     // world-space center
    glm::vec3 rotation{0.0f};     // euler angles in degrees

    // Shape parameters (use what applies to the active shape)
    glm::vec3 halfExtents{1.0f};  // Box: half-sizes on each axis
    float     radius     = 1.0f;  // Sphere: radius; Cylinder: radius; Capsule: radius
    float     halfHeight = 1.0f;  // Cylinder: half-height; Capsule: cylinder half-height

    // Trigger runtime flags (only relevant for Trigger / SolidAndTrigger modes)
    bool fireOnce     = false;
    bool enabled      = true;
    bool playerInside = false;    // runtime: is player currently overlapping
    bool pendingEnter = false;    // set by PhysicsSystem, consumed by BehaviorSystem
    bool pendingExit  = false;    // set by PhysicsSystem, consumed by BehaviorSystem
};
```

**Note on D-09 correction:** The user decision says "PhysicsSystem uses Jolt `ContactListener`" but the Jolt API makes this impossible for `CharacterVirtual`. The global `ContactListener` is NOT called for virtual character vs. sensor body contacts. Sensor detection for this project's player MUST use `CharacterContactListener`. This is a technical correction the planner must account for — implement via `CharacterContactListener`, not the global one.

### Pattern 1: Jolt Sensor Body Creation

**What:** Create a static Jolt body with `mIsSensor = true`. Sensor bodies generate contact callbacks but no collision response.
**When to use:** For any `ColliderComponent` with `mode == Trigger` or `mode == SolidAndTrigger`.

```cpp
// Source: Jolt Physics SensorTest.cpp + BodyCreationSettings.h (Jolt v5.4.0)
// For Trigger-only bodies:
JPH::BodyCreationSettings sensorSettings(
    shape,
    toJoltR(collider.position),
    toJoltQuat(collider.rotation),
    JPH::EMotionType::Static,
    Layers::NON_MOVING          // reuse existing layer — sensors collide with MOVING
);
sensorSettings.mIsSensor = true;
// mUserData encodes the entt::entity for lookup in the contact callback
sensorSettings.mUserData = static_cast<uint64_t>(entity);

JPH::BodyID sensorBodyId = bodyInterface.CreateAndAddBody(sensorSettings, JPH::EActivation::DontActivate);
```

**Jolt sensor documentation (from Body.h):**
> "The cheapest sensor (in terms of CPU usage) is a sensor with motion type Static. These sensors will only detect collisions with active Dynamic or Kinematic bodies."

The character controller uses `CharacterVirtual` which is NOT a rigid body — it's a virtual character. But from the CharacterVirtual source: sensors ARE detected and reported via `CharacterContactListener::OnContactAdded`.

### Pattern 2: CharacterContactListener for Sensor Events

**What:** Subclass `JPH::CharacterContactListener` and register it on the `CharacterVirtual`. When `mIsSensorB == true` on the Contact, look up the sensor entity and set flags.
**When to use:** This is the ONLY path to detect CharacterVirtual vs. sensor body overlap in Jolt. The global `ContactListener` is not called for CharacterVirtual contacts.

```cpp
// Source: Jolt Physics CharacterVirtual.h + CharacterVirtualTest.cpp (Jolt v5.4.0)
// Inner class in PhysicsSystem::Impl:

class SensorContactListener : public JPH::CharacterContactListener {
public:
    struct PendingContact {
        entt::entity sensorEntity;
        bool         isEnter; // true = enter, false = exit
    };

    // Called by CharacterVirtual::ExtendedUpdate() per contact
    void OnContactAdded(const JPH::CharacterVirtual* /*inCharacter*/,
                        const JPH::BodyID& inBodyID2,
                        const JPH::SubShapeID& /*inSubShapeID2*/,
                        JPH::RVec3Arg /*inContactPosition*/,
                        JPH::Vec3Arg /*inContactNormal*/,
                        JPH::CharacterContactSettings& /*ioSettings*/) override
    {
        // Look up sensor entity by BodyID
        auto it = sensorBodyToEntity.find(inBodyID2);
        if (it != sensorBodyToEntity.end()) {
            pending.push_back({it->second, /*isEnter=*/true});
        }
    }

    void OnContactRemoved(const JPH::CharacterVirtual* /*inCharacter*/,
                          const JPH::BodyID& inBodyID2,
                          const JPH::SubShapeID& /*inSubShapeID2*/) override
    {
        auto it = sensorBodyToEntity.find(inBodyID2);
        if (it != sensorBodyToEntity.end()) {
            pending.push_back({it->second, /*isEnter=*/false});
        }
    }

    // Populated by PhysicsSystem when sensor bodies are created/destroyed
    std::unordered_map<JPH::BodyID, entt::entity, BodyIDHash> sensorBodyToEntity;
    std::vector<PendingContact> pending; // drained each frame after ExtendedUpdate
};
```

After `character->ExtendedUpdate(...)` returns, drain `pending` and set `pendingEnter`/`pendingExit` flags on `ColliderComponent`:

```cpp
// After ExtendedUpdate():
for (const auto& contact : impl_->sensorListener.pending) {
    auto* collider = registry.try_get<ColliderComponent>(contact.sensorEntity);
    if (collider == nullptr || !collider->enabled) continue;
    if (contact.isEnter) {
        collider->playerInside = true;
        collider->pendingEnter = true;
        if (collider->fireOnce) collider->enabled = false;
    } else {
        collider->playerInside = false;
        collider->pendingExit  = true;
    }
}
impl_->sensorListener.pending.clear();
```

**Threading constraint (from ContactListener.h):**
> "contact listener callbacks are called from multiple threads at the same time when all bodies are locked"

For CharacterContactListener this constraint also applies. The `pending` vector MUST only be written from the contact callback thread and read AFTER `ExtendedUpdate()` completes on the same thread. Since the project uses a single-threaded update loop, this is safe without locks.

### Pattern 3: SolidAndTrigger Body Implementation

**What:** `SolidAndTrigger` mode creates TWO Jolt bodies for the same entity — one regular static body (collision response) and one sensor body (contact events). They share the same shape and position.
**When to use:** Only for `ColliderMode::SolidAndTrigger`.

**Alternative approach:** Create a single non-sensor static body. Separately check player proximity using `PhysicsSystem::WereBodiesInContact()`. This is simpler (one body) but only works when the player has an inner rigid body.

**Recommended approach:** Two bodies (one static NON_MOVING + one sensor NON_MOVING). The solid body blocks movement. The sensor body fires callbacks. Both are tracked in `Impl::staticBodies` and `Impl::sensorBodies` maps respectively. Destroyed together on entity removal.

```
struct DualBodyBinding {
    JPH::BodyID solidBodyId;
    JPH::BodyID sensorBodyId;
};
```

### Pattern 4: Unified makeColliderShape() Helper

Extend the existing `makeColliderShape()` free function in `PhysicsSystem.cpp`:

```cpp
// Source: derived from existing PhysicsSystem.cpp + Jolt SphereShape.h + CapsuleShape.h
JPH::ShapeRefC makeColliderShape(const ColliderComponent& collider) {
    switch (collider.shape) {
    case ColliderShape::Box:
        return new JPH::BoxShape(toJolt(collider.halfExtents));
    case ColliderShape::Cylinder:
        return new JPH::CylinderShape(collider.halfHeight, collider.radius);
    case ColliderShape::Sphere:
        return new JPH::SphereShape(collider.radius);
    case ColliderShape::Capsule:
        return new JPH::CapsuleShape(collider.halfHeight, collider.radius);
    }
    return nullptr;
}
```

Note: `CapsuleShape(halfHeightOfCylinder, radius)` — the halfHeight parameter is the half-height of the **cylindrical part only**, not including the spherical end caps. The total height of a capsule is `2 * halfHeight + 2 * radius`. This matches `CharacterVirtual`'s existing capsule usage in `PhysicsSystem.cpp`.

### Pattern 5: Scene File Unified Format

New keyword `collider` with shape and mode tokens:

```
# Unified collider syntax (new):
collider box solid 0.0 -0.05 2.0 5.0 0.05 4.0 node n_col_floor
collider cylinder solid 0.0 1.0 0.0 0.5 2.0 node n_pillar
collider box trigger 5.0 1.0 -3.0 2.0 1.5 1.0 node trigger_01 fire_once
collider sphere trigger -2.0 0.0 4.0 3.5 node trigger_02
collider capsule solidandtrigger 0.0 1.0 0.0 0.4 1.2 node gate_01

# Old syntax (kept for backward compat during migration):
collider_box 0.0 -0.05 2.0 5.0 0.05 4.0 node n_col_floor   → parsed as collider box solid
collider_cylinder ...                                          → parsed as collider cylinder solid
trigger_box ...                                                → parsed as collider box trigger
trigger_sphere ...                                             → parsed as collider sphere trigger
```

Positional parameters by shape:
- `box`: `hx hy hz` (half-extents)
- `sphere`: `radius`
- `cylinder`: `radius half_height`
- `capsule`: `radius half_height`

Optional modifiers: `rotation rx ry rz`, `node <id>`, `parent <id>`, `fire_once`

**Serializer:** The `serializeLevelDef()` function emits only the new `collider` keyword for all placements. Old keywords are only parsed (not written). This means once any scene is saved through the editor or serializer, it upgrades to the new format.

### Anti-Patterns to Avoid

- **Using the global JPH::ContactListener for player-trigger detection:** CharacterVirtual does not generate global contact listener events. Only `CharacterContactListener` receives them.
- **Creating a Dynamic sensor body for static triggers:** Static sensor bodies are cheaper (CPU) and sufficient. Dynamic sensors require activation and can go to sleep unexpectedly.
- **Calling physics API from inside contact callbacks:** The docs state all bodies are locked during contact callbacks. Only read body properties — never call `bodyInterface.RemoveBody()` or similar from a contact callback. Defer to post-update processing.
- **Forgetting to destroy sensor bodies on entity removal:** The cleanup loop in `update()` checks entity validity. It must handle both `solidBodies` and `sensorBodies` maps for `ColliderMode::SolidAndTrigger`.
- **Checking `mIsSensorB` for the enter/exit distinction:** `CharacterContactListener::OnContactAdded` fires for all contacts, not just sensors. For sensor entry/exit specifically, all contacts on a sensor body will have `mIsSensorB == true` — but the flag does not need to be checked in the listener because the lookup map (`sensorBodyToEntity`) only contains sensor bodies.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Sphere vs AABB overlap for triggers | Custom AABB/sphere math (existing TriggerSystem) | Jolt sensor bodies + CharacterContactListener | Handles rotation, complex shapes, broadphase optimization automatically |
| Capsule shape math | Custom intersection code | `JPH::CapsuleShape(halfHeight, radius)` | Edge cases in swept capsule detection are notoriously hard |
| Body-to-entity mapping | Global entity tag stored on component | `BodyCreationSettings::mUserData` = entt::entity cast | Jolt provides a 64-bit user data field exactly for this |
| Contact tracking (enter vs exit) | Manual "was inside last frame" state | `CharacterContactListener::OnContactAdded` / `OnContactRemoved` | Jolt tracks contact persistence across frames, fires Removed when contact ends |

---

## Common Pitfalls

### Pitfall 1: CharacterVirtual vs Global ContactListener Confusion
**What goes wrong:** D-09 in the CONTEXT.md says "uses Jolt ContactListener" — implementing this with `physicsSystem->SetContactListener()` will produce a listener that never fires for player-trigger interactions, because `CharacterVirtual` is not a rigid body in the broad phase.
**Why it happens:** The distinction between `JPH::ContactListener` (rigid body vs rigid body) and `JPH::CharacterContactListener` (virtual character vs rigid body) is not obvious from the Jolt API surface.
**How to avoid:** Register a `CharacterContactListener` on each `CharacterVirtual` via `character->SetListener(&sensorListener)`. The `CharacterVirtual::ContactAdded()` internal method calls this listener for every contact including sensors.
**Warning signs:** If OnContactAdded is never called when the player walks through a trigger, the wrong listener type is being used.

### Pitfall 2: Sensor Body Layer Collision Matrix
**What goes wrong:** Static sensor bodies in `Layers::NON_MOVING` will not detect the player because the current `OLPairFilterImpl::ShouldCollide()` does NOT allow `NON_MOVING` vs `NON_MOVING` (and CharacterVirtual uses MOVING layer).
**Why it happens:** Current `ShouldCollide(NON_MOVING, MOVING)` returns `true` for static vs dynamic — but CharacterVirtual sweeps against all layers that it finds via `mObjectLayer` on its body. Sensors in `NON_MOVING` WILL be found because MOVING objects collide with NON_MOVING.
**Verdict (verified in CharacterVirtual.cpp line 357-360):** CharacterVirtual's sweep code checks `body.IsSensor()` and skips sensors — but `OnContactAdded` is still called with `mIsSensorB = true` even for sensors found during the sweep. So placing sensor bodies in `Layers::NON_MOVING` works correctly. No new SENSOR layer is needed.
**How to avoid:** Place all sensor bodies in `Layers::NON_MOVING` (same as solid static bodies). The existing two-layer setup is sufficient.

### Pitfall 3: SolidAndTrigger Body Lifecycle Sync
**What goes wrong:** If the solid body and sensor body get out of sync (one destroyed, other not), you get a floating sensor with no physics body or a blocking wall with no trigger.
**Why it happens:** The cleanup loop in `update()` currently checks for `StaticColliderComponent` removal to decide when to destroy bodies. With a unified component, there's one component controlling two bodies.
**How to avoid:** Track `DualBodyBinding{solidBodyId, sensorBodyId}` in a single map entry. The cleanup condition is simply "entity no longer has `ColliderComponent`". Remove both body IDs atomically in the cleanup loop.

### Pitfall 4: CharacterContactListener Lifetime
**What goes wrong:** `SensorContactListener` is a member of `PhysicsSystem::Impl`. If `Impl` is reset before `CharacterVirtual` is destroyed, the character holds a dangling pointer to the listener.
**Why it happens:** `CharacterVirtual` stores the listener pointer raw; it does not own it.
**How to avoid:** In `shutdownRuntime()`, call `character->SetListener(nullptr)` on all characters before destroying the `Impl`. The existing shutdown order already destroys characters before resetting `impl_`, so this is just an explicit null assignment before `impl_.reset()`.

### Pitfall 5: Scene File Parser Breaking on New Keyword
**What goes wrong:** Adding a new `collider` keyword while the parser has `throwParseError(..., "unknown record type")` at the bottom means any unrecognized variant crashes.
**Why it happens:** The parser is strict by design.
**How to avoid:** Add the `collider` block handler before the `throwParseError` call. The old `collider_box` and `collider_cylinder` handlers stay in place to parse existing scene files. After the migration pass converts all scenes, the old handlers can be removed in a follow-up.

### Pitfall 6: Editor Capsule Wireframe Using Cube Approximation
**What goes wrong:** The existing editor renders all trigger shapes as scaled cubes (even sphere triggers). This is documented in the source as "approximated as scaled cube". A capsule rendered as a cube looks wrong and misleads level designers.
**Why it happens:** The renderer only has a `cube` mesh for wireframe visualization.
**How to avoid:** For capsule mode, render two stacked cubes (cylinder portion) plus sphere approximation handles at caps — or accept cube approximation with a larger scale for now. The discretion area says "capsule wireframe" is Claude's call. Recommended: approximate with cube at `(radius*2, (halfHeight+radius)*2, radius*2)` scale, identical to how sphere is handled today. Add a comment noting it's an approximation.

---

## Code Examples

### Creating a Sensor Body (Trigger mode)

```cpp
// Source: Jolt SensorTest.cpp + BodyCreationSettings.h (v5.4.0)
// Use this in PhysicsSystem::update() when a ColliderComponent with mode != Solid is found

JPH::ShapeRefC shape = makeColliderShape(collider); // existing helper, extended
if (!shape) continue;

JPH::BodyCreationSettings sensorSettings(
    shape,
    toJoltR(collider.position),
    toJoltQuat(collider.rotation),
    JPH::EMotionType::Static,
    Layers::NON_MOVING          // works with existing 2-layer setup
);
sensorSettings.mIsSensor = true;
sensorSettings.mUserData = static_cast<JPH::uint64>(static_cast<std::uint32_t>(entity));

JPH::BodyID sensorBodyId = bodyInterface.CreateAndAddBody(
    sensorSettings, JPH::EActivation::DontActivate);
if (!sensorBodyId.IsInvalid()) {
    impl_->sensorBodies.emplace(entity, Impl::SensorBodyBinding{sensorBodyId});
    impl_->sensorListener.sensorBodyToEntity.emplace(sensorBodyId, entity);
}
```

### Registering the CharacterContactListener

```cpp
// Source: Jolt CharacterVirtual.h — SetListener() (v5.4.0)
// After creating CharacterVirtual in PhysicsSystem::update():
binding.character->SetListener(&impl_->sensorListener);
```

### LevelDef Migration: New LevelColliderPlacement struct

```cpp
// Replaces LevelBoxColliderPlacement + LevelCylinderColliderPlacement + TriggerPlacement
struct LevelColliderPlacement {
    ColliderShape shape  = ColliderShape::Box;
    ColliderMode  mode   = ColliderMode::Solid;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 halfExtents{1.0f}; // Box
    float     radius     = 1.0f; // Sphere, Cylinder, Capsule
    float     halfHeight = 1.0f; // Cylinder, Capsule
    bool      fireOnce   = false;
    std::string nodeId;
    std::string parentNodeId;
    std::vector<BehaviorDeclaration> behaviors;
};
```

The `LevelDef` struct gains `std::vector<LevelColliderPlacement> colliders` and removes `boxColliders`, `cylinderColliders`, and `triggers`.

### BehaviorSystem Update (minimal)

```cpp
// Before (reads TriggerComponent):
auto view = registry.view<TriggerComponent, BehaviorComponent>();
for (auto [entity, trigger, behavior] : view.each()) {
    if (trigger.pendingEnter) { ... trigger.pendingEnter = false; }
    if (trigger.pendingExit)  { ... trigger.pendingExit  = false; }
}

// After (reads ColliderComponent, only entities with behavior):
auto view = registry.view<ColliderComponent, BehaviorComponent>();
for (auto [entity, collider, behavior] : view.each()) {
    if (collider.mode == ColliderMode::Solid) continue; // no trigger events
    if (collider.pendingEnter) { ... collider.pendingEnter = false; }
    if (collider.pendingExit)  { ... collider.pendingExit  = false; }
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual AABB/sphere overlap in TriggerSystem | Jolt sensor bodies + CharacterContactListener | This phase | Rotation support, broadphase acceleration, single codepath |
| Two separate components (StaticCollider + Trigger) | One ColliderComponent with mode enum | This phase | Single ECS component, unified serialization |
| Three editor object kinds (BoxCollider, CylinderCollider, Trigger) | One Collider kind with mode dropdown | This phase | Simpler editor data model |

**Current approach notes:**
- `StaticColliderComponent` already uses Jolt (good — no migration path needed for solid bodies)
- `TriggerComponent` does NOT use Jolt (being replaced by sensor bodies)
- `CharacterVirtual` is the only moving body in the simulation — all sensor detection goes through it

---

## Open Questions

1. **CharacterContactListener OnContactPersisted vs OnContactAdded for enter detection**
   - What we know: `OnContactAdded` fires when a contact is new. `OnContactPersisted` fires for contacts that existed last frame and still exist. `OnContactRemoved` fires when contact ends.
   - What's unclear: The Jolt docs note that for `EMotionQuality::LinearCast` bodies, an `OnContactAdded` can be followed by `OnContactPersisted` in the same update. CharacterVirtual uses Discrete quality, so this should not apply.
   - Recommendation: Implement only `OnContactAdded` for enter and `OnContactRemoved` for exit. Do NOT set `pendingEnter` in `OnContactPersisted` — that would re-fire enter every frame while the player stands inside the trigger.

2. **`fireOnce` flag atomicity when multiple players are added in future**
   - What we know: Currently only one `CharacterVirtual` (the player). `fireOnce` disables the collider after first enter.
   - What's unclear: If ever multi-character, the same sensor fires once then goes silent for the second character.
   - Recommendation: Not a concern for Phase 17. Single-player game. Document the assumption in ColliderComponent.

3. **SolidAndTrigger mode for the existing door/wall colliders**
   - What we know: None of the existing scene files use `SolidAndTrigger`. It's a new capability.
   - What's unclear: Whether the editor inspector should visually distinguish SolidAndTrigger from Solid (e.g., show behavior section only for modes with trigger capability).
   - Recommendation: Show behavior section in the inspector only when `mode != Solid`. This matches the Source engine pattern where only trigger brushes have outputs.

---

## Migration Inventory

This is a refactor/unification phase. The following enumerates all existing state that must be updated:

| Category | Items | Action Required |
|----------|-------|-----------------|
| **Components** | `StaticColliderComponent.h` — deleted; `TriggerComponent.h` — deleted | Replace with `ColliderComponent.h` |
| **Systems** | `TriggerSystem.h/.cpp` — deleted | Sensor detection moves into PhysicsSystem |
| **System registration** | `apps/runtime/main.cpp` — `TriggerSystem` registered as a system | Remove TriggerSystem from system list |
| **LevelDef structs** | `LevelBoxColliderPlacement`, `LevelCylinderColliderPlacement`, `TriggerPlacement` — all removed from `LevelDef.h` | Replace with `LevelColliderPlacement` |
| **LevelDef parser** | `collider_box`, `collider_cylinder`, `trigger_box`, `trigger_sphere` handlers in `LevelDef.cpp` | Keep old handlers for backward compat, add new `collider` handler |
| **LevelDef serializer** | `serializeLevelDef()` emits `collider_box`/`collider_cylinder`/`trigger_box`/`trigger_sphere` | Rewrite to emit `collider <shape> <mode> ...` |
| **LevelBuilder** | `addCollider()` (box), `addCollider()` (cylinder), `addTrigger()` methods | Merge into single `addCollider(const LevelColliderPlacement&)` |
| **Scene files** | 6 `.scene` files — `collider_box`/`collider_cylinder` lines (triggers: none found) | Parser backward compat handles reading; save from editor upgrades format |
| **Editor kind enum** | `EditorSceneObjectKind::BoxCollider`, `CylinderCollider`, `Trigger` — three kinds | Consolidate to single `EditorSceneObjectKind::Collider` |
| **Editor payload variant** | `LevelBoxColliderPlacement`, `LevelCylinderColliderPlacement`, `TriggerPlacement` in variant | Replace with `LevelColliderPlacement` |
| **Editor add methods** | `addBoxCollider()`, `addCylinderCollider()`, `addTrigger()` | Replace with `addCollider(const LevelColliderPlacement&)` |
| **Editor inspector** | Separate collider/trigger inspector sections | Single inspector with shape + mode dropdowns |
| **Editor preview renderer** | Separate trigger wireframe rendering (green=box, blue=sphere) | Unified wireframe with color by mode (grey=Solid, green=Trigger, yellow=SolidAndTrigger) |
| **BehaviorSystem** | `#include "game/behavior/TriggerComponent.h"` + `registry.view<TriggerComponent, ...>()` | Change include + view type to `ColliderComponent` |
| **Tests** | `test_behavior_trigger_roundtrip.cpp` — creates TriggerPlacement | Rewrite to use LevelColliderPlacement |
| **Tests** | `test_level_roundtrip.cpp` — may test box/cylinder colliders | Update placement struct types |
| **Build system** | CMakeLists.txt / compile targets that list TriggerSystem.cpp | Remove TriggerSystem.cpp from source list |

**Nothing found in stored data/live config/OS-registered state categories** — this is a source code + scene file migration only, no external system state.

---

## Environment Availability

Step 2.6: SKIPPED — no external dependencies beyond the already-vendored Jolt Physics. All required Jolt headers (`SphereShape.h`, `CapsuleShape.h`, `CharacterVirtual.h`) are present in the vendored Jolt source at `/build-test/_deps/joltphysics-src/`.

---

## Sources

### Primary (HIGH confidence)
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Collision/ContactListener.h` — ContactListener API, threading constraints, OnContactAdded/Persisted/Removed signatures
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Body/BodyCreationSettings.h` — `mIsSensor` field (line 96), `mUserData` field
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Body/Body.h` — `SetIsSensor()`, `IsSensor()`, static sensor description
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Character/CharacterVirtual.h` — `CharacterContactListener`, `mIsSensorB` on Contact struct, `SetListener()`
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Character/CharacterVirtual.cpp` — Lines 709-710: sensors get `OnContactAdded` then are ignored for physics response
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Collision/Shape/SphereShape.h` — `SphereShape(radius)` constructor
- `/build-test/_deps/joltphysics-src/Jolt/Physics/Collision/Shape/CapsuleShape.h` — `CapsuleShape(halfHeightOfCylinder, radius)` constructor, parameter naming
- `/build-test/_deps/joltphysics-src/Samples/Tests/General/SensorTest.cpp` — canonical sensor creation pattern, SENSOR layer usage
- `/build-test/_deps/joltphysics-src/Samples/Layers.h` — SENSOR ObjectLayer and BroadPhaseLayer definitions
- `/build-test/_deps/joltphysics-src/Samples/Tests/Character/CharacterVirtualTest.cpp` — CharacterContactListener usage with sensors, OnContactAdded implementation

### Secondary (HIGH confidence — project source)
- `src/engine/physics/PhysicsSystem.cpp` — existing `makeColliderShape()`, body lifecycle, character controller creation pattern
- `src/game/components/StaticColliderComponent.h` — current ColliderShape enum (Box/Cylinder)
- `src/game/behavior/TriggerComponent.h` — current flags: playerInside, pendingEnter, pendingExit, fireOnce
- `src/game/behavior/TriggerSystem.cpp` — manual overlap logic being deleted
- `src/game/behavior/BehaviorSystem.cpp` — `processTriggerFlags()` consuming flags (lines 59-73)
- `src/game/level/LevelDef.h` — existing placement structs, LevelDef layout
- `src/game/level/LevelDef.cpp` — parser (lines 728-1003) and serializer (lines 1306-1413)
- `src/editor/scene/EditorSceneDocument.h` — EditorSceneObjectKind enum, payload variant
- `src/editor/render/EditorScenePreviewRenderer.cpp` — trigger wireframe rendering pattern

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all Jolt APIs verified directly from vendored source headers
- Architecture patterns: HIGH — CharacterContactListener mechanism confirmed via CharacterVirtual.cpp source
- Pitfalls: HIGH — ContactListener vs CharacterContactListener distinction confirmed from Jolt source
- Migration inventory: HIGH — every affected file identified and read

**Research date:** 2026-04-05
**Valid until:** 2026-07-05 (Jolt v5.4.0 is vendored; API won't change until manual upgrade)
