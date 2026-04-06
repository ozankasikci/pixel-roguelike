# Quick Task 260406-vdu: Fix SingleDoor Architecture - Context

**Gathered:** 2026-04-06
**Status:** Ready for planning

<domain>
## Task Boundary

Rethink SingleDoor as a proper multi-part object with correct pivot and rotation. The current architecture treats a door as a single scene object (`LevelSingleDoorPlacement` / `EditorSceneObjectKind::SingleDoor`) that internally spawns two ECS entities (frame mesh + leaf mesh with `EditorDoorLeafTag`). This causes problems: hinge pivot math is hacky, changing the pivot moves the door, and parts can't be independently inspected.

</domain>

<decisions>
## Implementation Decisions

### Object Model
- **Split into parts**: Frame and leaf become separate Mesh scene objects inside a Group. Door behavior (interaction, animation) comes from a component/metadata on the group. Each part is a real scene object with its own transform.
- Remove `LevelSingleDoorPlacement` and `EditorSceneObjectKind::SingleDoor` in favor of the group-of-meshes model.

### Gizmo & Selection
- **Select the group**: Clicking any door part in the viewport selects the parent Door group. The gizmo moves/rotates the whole assembly. Users can expand the group in the outliner to select individual parts (frame or leaf) for fine-tuning.

### Pivot Behavior
- **Leaf has its own pivot**: The leaf mesh object gets a pivot/origin property. The gizmo rotates the leaf around its pivot. The frame has no special pivot needs. Runtime door animation (open/close) rotates the leaf around the same pivot property.

### Claude's Discretion
- Migration strategy for existing `.scene` files (in-place upgrade vs manual recreation)
- Whether pivot is a general Mesh property or door-leaf-specific
- Runtime DoorComponent wiring (how the group's children get linked to the animation system)

</decisions>

<specifics>
## Specific Ideas

- Current hardcoded hinge offset is `(-0.45, 0, 0.04)` — this should become the leaf's pivot
- The `hingePivot` field added in 260406-uqb can be migrated to the leaf's pivot property
- Existing `EditorDoorLeafTag` marker pattern should be replaced by the leaf being its own scene object
- The runtime `DoorLeafComponent` + `DoorComponent` already models multi-part doors — the editor should match this

</specifics>

<canonical_refs>
## Canonical References

- `src/game/level/LevelDef.h` — LevelSingleDoorPlacement struct (to be replaced)
- `src/game/level/LevelDef.cpp` — Parser/serializer for single_door records
- `src/editor/scene/EditorPreviewWorld.cpp` — rebuild() and syncTransforms() SingleDoor cases
- `src/editor/scene/EditorSceneDocument.cpp` — All SingleDoor handling (addSingleDoor, applyWorldTransform, localTransformMatrix, etc.)
- `src/editor/ui/inspectors/SingleDoorInspector.cpp` — Inspector panel for doors
- `src/game/prefabs/GameplayPrefabs.cpp` — Runtime door spawning
- `src/game/runtime/RuntimeGameplay.cpp` — updateRuntimeDoors() with DoorLeafComponent

</canonical_refs>
