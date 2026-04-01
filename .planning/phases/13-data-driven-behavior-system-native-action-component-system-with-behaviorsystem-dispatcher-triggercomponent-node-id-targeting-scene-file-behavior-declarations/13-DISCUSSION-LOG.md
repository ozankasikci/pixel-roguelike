# Phase 13: Data-driven Behavior System - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-02
**Phase:** 13-data-driven-behavior-system
**Areas discussed:** Action type scope, Scene file syntax, Migration strategy, Trigger volumes

---

## Action Type Scope

### Q1: Which action types should be in the initial set?

| Option | Description | Selected |
|--------|-------------|----------|
| Core interactions | OpenDoor, CloseDoor, ToggleDoor, PlaySound, SetLight, FlickerLight, ShowMessage, Delay | ✓ |
| Entity control | EnableEntity, DisableEntity, EmitEvent | ✓ |
| Player control | LockPlayer, UnlockPlayer, TeleportPlayer | ✓ |
| Scene transitions | ChangeScene — load a different .scene file | |

**User's choice:** Core interactions + Entity control + Player control (3 of 4 selected)
**Notes:** Scene transitions deferred — not needed for current content

### Q2: Action parameter representation

| Option | Description | Selected |
|--------|-------------|----------|
| Generic parameter slots | paramFloat1-3, paramString, paramVec3 — simple, covers all types | |
| Typed std::variant | DoorActionParams, SoundActionParams, LightActionParams — compile-time safety | ✓ |
| You decide | Claude picks | |

**User's choice:** Typed std::variant
**Notes:** User prefers compile-time type safety over simplicity

---

## Scene File Syntax

### Q1: How should behavior declarations appear in .scene files?

| Option | Description | Selected |
|--------|-------------|----------|
| Flat prefixed lines | behavior n_door_a on_activate open_door ... — no parser changes | |
| Indented sub-lines | 2-space indent under parent entity line — requires parser refactoring | ✓ |
| Separate .behavior files | One file per entity/group — adds file management overhead | |

**User's choice:** Indented sub-lines
**Notes:** User chose the option that requires parser refactoring for cleaner syntax

### Q2: Should interactable also move to sub-line syntax?

| Option | Description | Selected |
|--------|-------------|----------|
| Move interactable to sub-lines | Consistent syntax, requires .scene migration | ✓ |
| Keep interactable flat | Less migration, inconsistent syntax | |

**User's choice:** Move interactable to sub-lines too
**Notes:** Consistency preferred — all entity metadata indented under parent

---

## Migration Strategy

### Q1: What happens to DoorSystem and CheckpointSystem?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep animation, replace activation | BehaviorSystem dispatches, DoorAnimationSystem animates | ✓ |
| Delete entirely | All logic into BehaviorSystem | |
| Leave untouched | Only new behaviors use BehaviorSystem | |

**User's choice:** Keep animation, replace activation
**Notes:** Clean separation of concerns — activation dispatch vs animation update

### Q2: Migrate existing .scene files?

| Option | Description | Selected |
|--------|-------------|----------|
| Migrate existing scenes | Convert institutional_room.scene etc. to behavior sub-lines | ✓ |
| New scenes only | Existing scenes keep working via DoorComponent | |

**User's choice:** Migrate existing scenes
**Notes:** Proves the system end-to-end with real content

---

## Trigger Volumes

### Q1: Include triggers in this phase?

| Option | Description | Selected |
|--------|-------------|----------|
| Include triggers | TriggerComponent with box/sphere, TriggerSystem checks overlap | ✓ |
| Defer triggers | Only interaction-based activation this phase | |

**User's choice:** Include triggers

### Q2: Overlap detection approach?

| Option | Description | Selected |
|--------|-------------|----------|
| Manual AABB/sphere checks | Point-in-box/sphere test against player position | ✓ |
| Jolt Physics sensors | Register as Jolt sensor bodies | |

**User's choice:** Manual AABB/sphere checks
**Notes:** Keeps triggers lightweight, no physics dependency

### Q3: Trigger visualization?

| Option | Description | Selected |
|--------|-------------|----------|
| Invisible + editor debug wireframe | In-game invisible, editor shows wireframe | ✓ |
| Invisible only | No visual anywhere | |
| You decide | Claude picks | |

**User's choice:** Invisible + editor debug wireframe

### Q4: Trigger re-entry behavior?

| Option | Description | Selected |
|--------|-------------|----------|
| Both modes | fireOnce flag, default repeating | ✓ |
| fire_once only | All triggers one-shot | |

**User's choice:** Both modes (repeating default + fireOnce flag)

---

## Claude's Discretion

- Delayed action queue implementation (priority queue vs sorted vector)
- BehaviorComponent event list naming
- entt::meta vs switch dispatch
- Editor behavior display approach
- Trigger wireframe rendering technique

## Deferred Ideas

- ChangeScene action type
- Lua scripting layer (when behavior count exceeds ~30)
- Editor behavior inspector CRUD
- Visual scripting / node graph
