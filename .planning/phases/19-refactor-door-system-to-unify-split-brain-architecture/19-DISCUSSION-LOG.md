# Phase 19: Refactor door system to unify split-brain architecture - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-07
**Phase:** 19-refactor-door-system-to-unify-split-brain-architecture
**Areas discussed:** Unified door definition, Config/state split, Editor inspector UX, Runtime unification

---

## Unified Door Definition

### Leaf Count

| Option | Description | Selected |
|--------|-------------|----------|
| Always support dual leaves | Single unified struct always has leftLeaf and rightLeaf fields. Single-leaf doors just leave rightLeaf empty. | ✓ |
| Variable leaf list | std::vector<DoorLeafDef> instead of fixed left/right fields. More flexible but more complex. | |
| You decide | Claude picks the approach | |

**User's choice:** Always support dual leaves
**Notes:** Matches existing DoorComponent pattern.

### Hinge Specification

| Option | Description | Selected |
|--------|-------------|----------|
| Pivot field on leaf mesh | Keep LevelDoorGroupPlacement approach — leaf meshes declare a pivot offset. | ✓ |
| Explicit hinge positions | Keep DoubleDoorSpawnSpec approach — leftHingePosition and rightHingePosition as explicit vec3 fields. | |
| You decide | Claude picks based on integration | |

**User's choice:** Pivot field on leaf mesh
**Notes:** Already used by makePivotLeafModel().

### Archetype Path

| Option | Description | Selected |
|--------|-------------|----------|
| Remove entirely | Delete DoubleDoorSpawnSpec. All doors via unified struct in .scene files. | ✓ |
| Keep as thin wrapper | DoubleDoorSpawnSpec stays but constructs unified definition internally. | |
| You decide | Claude evaluates need | |

**User's choice:** Remove entirely

### Scene Syntax

| Option | Description | Selected |
|--------|-------------|----------|
| Keep door_group | Backward-compatible. Extend keyword to support both leaves. | |
| Rename to door | Cleaner naming per R6. Requires migrating all .scene files. | |
| You decide | Claude picks based on migration cost vs clarity | ✓ |

**User's choice:** You decide (Claude's discretion)

---

## Config/State Split

### Leaf Entity References

| Option | Description | Selected |
|--------|-------------|----------|
| In DoorConfigComponent | Leaf entities are structural, set once at spawn. | ✓ |
| In DoorStateComponent | Keep alongside progress/opening for animation system convenience. | |
| You decide | Claude picks based on access patterns | |

**User's choice:** In DoorConfigComponent

### Close Animation

| Option | Description | Selected |
|--------|-------------|----------|
| Opening only (current) | Progress 0→1 only. CloseDoor resets to 0. No smooth close. | |
| Bidirectional animation | Target state (open/closed), progress moves both directions. Smooth close. | ✓ |
| You decide | Claude evaluates current ToggleDoor behavior | |

**User's choice:** Bidirectional animation

---

## Editor Inspector UX

### Leaf Editing Depth

| Option | Description | Selected |
|--------|-------------|----------|
| Read-only leaf display | Inspector shows leaves and pivots. Editing requires selecting leaf mesh directly. | ✓ |
| Inline leaf editing | Editable pivot fields, 'Add Pivot' button, left/right role dropdown. | |
| You decide | Claude matches existing inspector patterns | |

**User's choice:** Read-only leaf display

### No Leaf Warning

| Option | Description | Selected |
|--------|-------------|----------|
| Yellow warning banner | Display warning text in inspector. User manually adds child meshes. | ✓ |
| Warning + quick-fix button | Warning plus 'Add Default Leaf' button for placeholder mesh. | |
| You decide | Claude picks based on existing warning patterns | |

**User's choice:** Yellow warning banner

---

## Runtime Unification

### Editor Preview Animation

| Option | Description | Selected |
|--------|-------------|----------|
| Use DoorAnimationSystem | Editor preview instantiates real system. Guarantees parity. | ✓ |
| Keep direct makePivotLeafModel | Editor continues calling makePivotLeafModel() directly. Simpler but separate path. | |
| You decide | Claude evaluates feasibility | |

**User's choice:** Use DoorAnimationSystem

### RuntimeGameplay Door Functions

| Option | Description | Selected |
|--------|-------------|----------|
| Delete all door functions | Remove updateRuntimeDoorAnimation() entirely. DoorAnimationSystem is single source. | ✓ |
| Keep as thin delegate | One-liner in RuntimeGameplay.cpp that calls DoorAnimationSystem::update(). | |
| You decide | Claude evaluates call site patterns | |

**User's choice:** Delete all door functions

---

## Claude's Discretion

- Scene file keyword choice (door_group vs door)
- Unified door placement struct internal structure
- DoorAnimationSystem editor vs game context detection
- DoorLeafComponent changes (if any)
- Exact component naming
- DoorActionParams handling

## Deferred Ideas

None — discussion stayed within phase scope.
