# Phase 17: Unified Collider System with Trigger Capabilities and Scripting Support - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-05
**Phase:** 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
**Areas discussed:** Unification approach, Scripting scope, Shape type expansion, Trigger physics migration

---

## Unification Approach

| Option | Description | Selected |
|--------|-------------|----------|
| Single ColliderComponent with mode flag | One component with ColliderMode (Solid/Trigger/SolidAndTrigger). Merges shape enums. Clean but biggest refactor. | ✓ |
| Keep separate, share shape types | StaticColliderComponent and TriggerComponent stay separate but use shared ColliderShape enum. Less risky. | |
| Migrate triggers into Jolt sensors | TriggerComponent stays separate but switches from manual overlap to Jolt sensor bodies. | |
| You decide | Claude picks the approach | |

**User's choice:** Single ColliderComponent with mode flag
**Notes:** None

### Follow-up: SolidAndTrigger mode

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, three modes: Solid/Trigger/SolidAndTrigger | Maximum flexibility. Matches Source engine's solid+trigger brush behavior. | ✓ |
| Just Solid and Trigger | Simpler. If you need both, attach two entities. | |

**User's choice:** Yes, three modes
**Notes:** None

### Follow-up: Migration strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Full replacement — delete both | New unified ColliderComponent replaces both. Migrate all scene files and code in one go. | ✓ |
| Phased migration with compatibility | Introduce alongside existing ones. Adapter code converts old components. | |
| You decide | Claude picks the migration strategy | |

**User's choice:** Full replacement — delete both
**Notes:** None

---

## Scripting Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Lua scripting integration | Embed Lua (via sol2 or LuaJIT). Full scripting language. | |
| Conditional behavior chains | Extend behavior system with conditional actions. No external language. | |
| Event-driven state machines | StateComponent with named states. Behaviors reference states as conditions. | |
| Defer scripting entirely | Focus this phase purely on collider unification. | ✓ |

**User's choice:** Defer scripting entirely
**Notes:** Phase 13 originally deferred Lua scripting until behavior count exceeds ~30 or conditional branching needed. User confirmed this phase should focus purely on collider unification.

---

## Shape Type Expansion

| Option | Description | Selected |
|--------|-------------|----------|
| Box (keep) | Already in both systems. Most common shape. | ✓ |
| Sphere (keep) | Already in triggers. Natural for proximity zones. | ✓ |
| Cylinder (keep) | Already in colliders. Good for pillars, columns. | ✓ |
| Capsule (new) | Useful for character-sized colliders. Jolt has native CapsuleShape. | ✓ |

**User's choice:** All four shapes (Box, Sphere, Cylinder, Capsule)
**Notes:** None

### Follow-up: Shape-mode restrictions

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, any shape in any mode | Maximum flexibility. Clean uniform API. | ✓ |
| Restrict some combinations | E.g., capsule only for triggers. Less testing surface. | |

**User's choice:** Any shape in any mode
**Notes:** None

---

## Trigger Physics Migration

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, Jolt sensors for all triggers | Broadphase acceleration, rotation support, consistent collision. Delete manual TriggerSystem. | ✓ |
| Hybrid: Jolt sensors for SolidAndTrigger, manual for pure Trigger | Two code paths. | |
| Keep manual checks, just unify the component | Smallest change but no rotation support for triggers. | |

**User's choice:** Jolt sensors for all triggers
**Notes:** None

### Follow-up: Event delivery to BehaviorSystem

| Option | Description | Selected |
|--------|-------------|----------|
| Contact listener callbacks on PhysicsSystem | Jolt ContactListener fires events. PhysicsSystem sets flags on ColliderComponent. | |
| EventBus events from PhysicsSystem | Publishes TriggerEnterEvent/TriggerExitEvent on EventBus. | |
| You decide | Claude picks the approach that fits the existing flag-based pattern | ✓ |

**User's choice:** You decide
**Notes:** Claude has discretion on the event delivery mechanism. Existing flag pattern (pendingEnter/pendingExit) should be preserved conceptually.

---

## Claude's Discretion

- Event delivery mechanism from Jolt sensor contacts to BehaviorSystem
- Jolt layer assignments for sensor bodies
- Contact listener implementation details
- Scene file syntax for unified collider format
- Editor gizmo rendering for new shapes (capsule wireframe)

## Deferred Ideas

- Scripting support (Lua, conditional chains, state machines) — deferred to future phase
- Mesh colliders — not needed for current scenes
- Dynamic/kinematic colliders — current colliders are all static
- Collision layers/masks — fine-grained collision filtering
