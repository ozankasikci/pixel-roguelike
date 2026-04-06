# Phase 19: Refactor SingleDoor into Multi-Part Group Architecture with Pivot-Based Rotation - Research

**Researched:** 2026-04-06
**Domain:** C++ custom game engine — ECS architecture, editor scene document, scene file serialization, door animation system
**Confidence:** HIGH

## Summary

The current `SingleDoor` system is a monolithic scene object (`LevelSingleDoorPlacement`, `EditorSceneObjectKind::SingleDoor`) that packages frame mesh, leaf mesh, pivot, and behavior parameters into one struct. This causes two compounding problems: (1) hinge pivot math is computed inline in multiple places (`spawnSingleDoor`, `EditorPreviewWorld::rebuild`, `EditorPreviewWorld::syncTransforms`) creating drift and inconsistency, and (2) the door "root position" conflates placement with geometry — changing `hingePivot` used to move the door visually.

The design decision (captured in `260406-vdu-CONTEXT.md`) is to replace the monolithic `LevelSingleDoorPlacement` with a **door group** — a `LevelGroupNode` with `door_behavior` metadata — containing two real child `LevelMeshPlacement` objects (frame + leaf). The leaf mesh gets a `pivot` property (replacing the hidden `hingePivot`). Runtime door animation (`DoorLeafComponent` / `DoorComponent`) already models this multi-part structure correctly; the gap is entirely on the data/editor side.

**Primary recommendation:** Introduce a new scene file keyword `door_group` (a typed group with behavior params), add `pivot` to `LevelMeshPlacement`, migrate 3 doors in `initial_scene.scene`, remove `LevelSingleDoorPlacement` and `EditorSceneObjectKind::SingleDoor` everywhere.

## Standard Stack

No new libraries are needed. This is a pure refactor within the existing codebase stack.

| Component | Current | After Refactor |
|-----------|---------|----------------|
| ECS | EnTT v3.16 | Unchanged |
| Scene serializer | LevelDef.cpp custom text format | Extended: new `door_group` keyword, `pivot` on mesh |
| Editor document | `EditorSceneDocument` + `EditorSceneObjectPayload` variant | Remove `LevelSingleDoorPlacement` variant, add `LevelDoorGroupPlacement` |
| Runtime spawning | `spawnSingleDoor()` in `GameplayPrefabs.cpp` | Rewrite to read mesh children + pivot from group |
| Animation | `DoorAnimationSystem`, `DoorLeafComponent`, `DoorComponent` | No structural change needed |

## Architecture Patterns

### Current Architecture (to be removed)

```
LevelSingleDoorPlacement {
    doorMeshName, frameMeshName
    doorMaterialId, frameMaterialId
    rootPosition, doorYawDegrees
    openAngle, openDuration
    interactDistance, interactDotThreshold
    locked, lockedPrompt
    doorTint, frameTint
    hingePivot    ← local offset, used in pivot math inline
    nodeId, parentNodeId
}
```

In the runtime, `spawnSingleDoor()` then manually computes:
- World hinge = rootPosition + rotate(hingePivot, doorYawDegrees)
- centerOffsetFromHinge = (0.445, 0, 0) (hardcoded)

In the editor, `EditorPreviewWorld` duplicates this same pivot math in `rebuild()` and `syncTransforms()` for the `EditorDoorLeafTag` entity.

### Target Architecture

```
LevelDoorGroupPlacement {         ← new struct (replaces LevelSingleDoorPlacement)
    name                          ← display label for outliner ("Door A")
    position                      ← world position of group root
    yawDegrees                    ← Y rotation of whole assembly
    openAngle, openDuration       ← door behavior params
    interactDistance, interactDotThreshold
    locked, lockedPrompt
    nodeId, parentNodeId
}
  └── child LevelMeshPlacement (frame mesh)
        meshId = "SM_FrameA"
        position = (0,0,0)   ← relative to group
        scale = (0.22,0.22,0.22)
        materialId, tint
  └── child LevelMeshPlacement (leaf mesh) with pivot property
        meshId = "SM_DoorA"
        position = (0,0,0)   ← relative to group
        scale = (0.22,0.22,0.22)
        pivot = (-0.45, 0, 0.04)  ← NEW field on LevelMeshPlacement
        materialId, tint
```

Scene file format (new):
```
door_group Door_A -5.0 0.0 1.0 90.0 open_angle 90.0 node n_doorA_group
    mesh SM_FrameA 0.0 0.0 0.0 0.22 0.22 0.22 0.0 0.0 0.0 material qdp_door_a parent n_doorA_group
    mesh SM_DoorA 0.0 0.0 0.0 0.22 0.22 0.22 0.0 0.0 0.0 material qdp_door_a pivot -0.45 0.0 0.04 parent n_doorA_group
```

The `door_group` keyword carries: name, position, yaw, and door behavior params (openAngle, openDuration, locked, etc.). The mesh children live as normal `mesh` records with `parent` pointing to the door group nodeId.

### Pivot Property on LevelMeshPlacement

The `pivot` field is a `std::optional<glm::vec3>` added to `LevelMeshPlacement`. When present, the runtime interprets it as the local-space hinge origin for `DoorLeafComponent`. The frame mesh has no pivot; only the leaf mesh carries one.

Scene file serialization: emit `pivot x y z` as an optional token on `mesh` lines when set. Parser reads it like `tint`.

### Runtime Wiring (spawnSingleDoor replacement)

`LevelBuilder` gains `addDoorGroup(const LevelDoorGroupPlacement&)`. This method:
1. Finds child mesh placements by `parentNodeId == doorGroup.nodeId`
2. Identifies leaf (has pivot) vs frame (no pivot) by checking `LevelMeshPlacement::pivot.has_value()`
3. Spawns frame as a normal static mesh entity
4. Spawns leaf with `DoorLeafComponent` populated from the leaf's world position, `pivot`, and the group's yaw
5. Spawns door root entity with `DoorComponent` + `InteractableComponent`

This eliminates all hardcoded `kDoorScale = 0.22f` — scale now lives in the mesh placement.

### Editor Object Model

`EditorSceneObjectKind` gets a new value `DoorGroup` (replacing `SingleDoor`). `EditorSceneObjectPayload` variant replaces `LevelSingleDoorPlacement` with `LevelDoorGroupPlacement`. Children (frame mesh, leaf mesh) are ordinary `Mesh` objects in the document with `parentNodeId` pointing to the door group.

Clicking either door child in the viewport selects the parent `DoorGroup` object (same behavior as group selection — clicks on children bubble up to the group root). Users expand the group in the outliner to select individual children for fine-tuning (pivot position).

`EditorPreviewWorld::rebuild()` case for `DoorGroup`:
- Does nothing for rendering (children render themselves as normal `Mesh` objects)
- The door group node itself spawns a zero-size entity only for ownerMap bookkeeping

`EditorPreviewWorld::syncTransforms()`:
- `Mesh` children with `pivot` use pivot-aware model: T(world_hinge) * R(yaw) * T(-pivot) * S(scale)
- `Mesh` children without `pivot` use the standard model matrix

This removes `EditorDoorLeafTag` entirely — the leaf is identified by `LevelMeshPlacement::pivot.has_value()`.

### Inspector

`SingleDoorInspector.cpp/.h` is replaced by `DoorGroupInspector.cpp/.h`. The door group inspector shows: name, position/yaw (via `drawTransformSection`), openAngle, openDuration, interactDistance, locked/lockedPrompt. It does NOT show mesh/material/tint — those are in the child mesh inspectors.

The leaf mesh inspector gains a pivot row (shows when `pivot.has_value()`, or always on a `DoorLeafMesh`). Simpler approach: the standard `MeshInspector` always shows `Pivot` row — it's just a `vec3` that defaults to `{0,0,0}` for non-door meshes.

### Scene Migration

3 existing `single_door` records in `initial_scene.scene` need converting to the new format. Migration is done manually (not an automatic upgrade path) since there are only 3 and they have well-known pivots.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead |
|---------|-------------|-------------|
| Pivot transform math | Custom hinge rotation functions per-callsite | Single `makePivotLeafModel(pos, pivot, yaw, scale)` helper, called from both `EditorPreviewWorld` and `spawnDoorGroup` |
| Scene format migration | A migration script | Manual edit of 3 `single_door` lines in `initial_scene.scene` |
| Leaf identification | Per-entity tags (`EditorDoorLeafTag`) | `LevelMeshPlacement::pivot.has_value()` check — data-driven |

## Common Pitfalls

### Pitfall 1: Pivot Math Drift (the reason this refactor exists)
**What goes wrong:** If pivot math is duplicated between `spawnDoorGroup()`, `EditorPreviewWorld::rebuild()`, and `EditorPreviewWorld::syncTransforms()`, they fall out of sync when the formula is updated in one place.
**Why it happens:** The old `spawnSingleDoor` computed hingeWorldPos inline; the preview world duplicated the same computation.
**How to avoid:** Extract a single `glm::mat4 makePivotLeafModel(glm::vec3 worldPos, glm::vec3 pivot, float yawDeg, glm::vec3 scale)` free function in a shared header (or anonymous namespace in a shared translation unit). Both the builder and the preview world call this same function.
**Warning signs:** Any callsite with `std::cos(yawRad)`, `std::sin(yawRad)`, and `pivot.x` that is not a call to the shared helper.

### Pitfall 2: centerOffsetFromHinge Hardcoding
**What goes wrong:** `DoorLeafComponent::centerOffsetFromHinge` is currently hardcoded to `(0.445f, 0, 0)` in `spawnSingleDoor`. This is the half-width of the SM_DoorA mesh at scale 0.22. After the refactor, scale comes from the mesh placement, so the offset must be computed from mesh bounds or stored explicitly.
**Why it happens:** The current code burns in mesh-specific geometry knowledge.
**How to avoid:** The center offset from hinge is the vector from the pivot to the mesh origin in mesh-local space. Since `pivot` is defined in mesh-local space (before scale), and the mesh origin is at (0,0,0) local, `centerOffsetFromHinge = -pivot * scale`. Verify this against the SM_DoorA geometry.
**Warning signs:** Door leaf visually offset from hinge when pivot is non-zero.

### Pitfall 3: resolveLevelHierarchy missing DoorGroup kind
**What goes wrong:** `resolveLevelHierarchy()` in `LevelDef.cpp` has a `LevelNodeRef::Kind` enum and a switch that handles each kind. If `DoorGroup` is not added, child meshes will not inherit the door group's world transform.
**Why it happens:** Adding a new placement type requires updating the enum, refs population loop, `localMatrixFor` lambda, and the resolve switch.
**How to avoid:** Add `LevelNodeRef::Kind::DoorGroup` and a `DoorGroup` case to every switch/if-chain in `resolveLevelHierarchy`.
**Warning signs:** Child meshes appear at world origin regardless of door group position.

### Pitfall 4: EditorSceneDocument applyWorldTransform / localTransformMatrix gaps
**What goes wrong:** `EditorSceneDocument` has a `localTransformMatrix()` private method and `applyWorldTransform()` public method, both dispatching on object kind via `std::visit`. Missing the new `LevelDoorGroupPlacement` case causes a compile-time `static_assert(sizeof(T) == 0)` failure (which is good) — but only if the new type is added to the `EditorSceneObjectPayload` variant.
**Why it happens:** The variant-based dispatch is exhaustive at compile time, but only for types already in the variant.
**How to avoid:** After adding `LevelDoorGroupPlacement` to `EditorSceneObjectPayload`, let the compiler find every `static_assert` failure and fix each callsite.
**Warning signs:** Build fails with `Unhandled payload type` asserts — this is expected and correct, fix all of them.

### Pitfall 5: outliner / add-object UI gaps
**What goes wrong:** The outliner add-object flow (EditorOutlinerPanel) and viewport right-click menu reference `EditorSceneObjectKind` values. If `SingleDoor` is removed before all UI references are updated, the add-door button disappears without replacement.
**Why it happens:** Multiple UI panels have hardcoded kind-dispatch.
**How to avoid:** Search for all `EditorSceneObjectKind::SingleDoor` occurrences before removing the enum value. Replace with `DoorGroup` equivalents.
**Warning signs:** Compile error `use of undeclared identifier 'SingleDoor'` — good, follow the chain.

### Pitfall 6: DoorComponent leftLeaf / rightLeaf entity reference
**What goes wrong:** `DoorComponent` stores two entity references (`leftLeaf`, `rightLeaf`). For a single door, only `leftLeaf` is set; `rightLeaf = entt::null`. The animation system skips `entt::null` correctly. This is unchanged.
**Why it happens:** The ECS runtime already supports single-leaf doors; the new architecture just needs to populate `leftLeaf` correctly from the child mesh entity that has a `DoorLeafComponent`.
**How to avoid:** `addDoorGroup()` sets `DoorComponent.leftLeaf` to the spawned leaf entity; `rightLeaf` stays `entt::null`.

## Code Examples

### Pivot-Aware Leaf Model (shared helper to extract)

```cpp
// Source: current EditorPreviewWorld::syncTransforms (to be extracted)
// Both runtime builder and editor preview should call this.
inline glm::mat4 makePivotLeafModel(
    const glm::vec3& groupWorldPos,
    float groupYawDeg,
    const glm::vec3& pivot,   // mesh-local pivot = hingePivot
    const glm::vec3& scale)
{
    const float yawRad = glm::radians(groupYawDeg);
    const glm::vec3 hingeWorldPos = groupWorldPos + glm::vec3(
        pivot.x * std::cos(yawRad) - pivot.z * std::sin(yawRad),
        0.0f,
        pivot.x * std::sin(yawRad) + pivot.z * std::cos(yawRad));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), hingeWorldPos);
    model = glm::rotate(model, yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, -pivot);
    model = glm::scale(model, scale);
    return model;
}
```

### New LevelDoorGroupPlacement struct

```cpp
// Source: new struct to add to LevelDef.h
struct LevelDoorGroupPlacement {
    std::string name = "Door";
    glm::vec3 position{0.0f};
    float yawDegrees = 0.0f;
    float openAngle = 90.0f;
    float openDuration = 1.2f;
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    bool locked = false;
    std::string lockedPrompt = "E  This door is locked";
    std::string nodeId;
    std::string parentNodeId;
};
```

### LevelMeshPlacement with pivot

```cpp
// Source: extension to existing LevelMeshPlacement in LevelDef.h
struct LevelMeshPlacement {
    // ... existing fields ...
    std::optional<glm::vec3> pivot;   // NEW: hinge pivot for door leaf meshes
};
```

### Scene file format for door_group

```
# New format
door_group DoorA -5.0 0.0 1.0 90.0 node n_dg_doorA
mesh SM_FrameA 0.0 0.0 0.0 0.22 0.22 0.22 0.0 0.0 0.0 material qdp_door_a node n_frame_doorA parent n_dg_doorA
mesh SM_DoorA 0.0 0.0 0.0 0.22 0.22 0.22 0.0 0.0 0.0 material qdp_door_a pivot -0.45 0.0 0.04 node n_leaf_doorA parent n_dg_doorA
```

Note: the door group's yaw IS the assembly yaw. Child meshes store local-space positions (0,0,0 for both — they use the group transform). The pivot is in mesh-local space relative to the mesh origin.

### Runtime addDoorGroup wiring

```cpp
// Source: new method in LevelBuilder (replaces addSingleDoor)
entt::entity LevelBuilder::addDoorGroup(
    const LevelDoorGroupPlacement& group,
    const LevelMeshPlacement& framePlacement,
    const LevelMeshPlacement& leafPlacement)
{
    // Frame: static mesh, no door components
    // World pos = group.position (frame is at group root)
    builder.addMesh(...frame...);

    // Leaf: compute hinge world pos from group + leaf.pivot
    const glm::vec3& pivot = leafPlacement.pivot.value_or(glm::vec3(0.0f));
    const glm::mat4 leafModel = makePivotLeafModel(
        group.position, group.yawDegrees, pivot, leafPlacement.scale);

    auto leafEntity = builder.addMesh(...leaf...);
    // Apply modelOverride; attach DoorLeafComponent
    DoorLeafComponent doorLeaf;
    doorLeaf.hingePosition = hingeWorldPos;     // from makePivotLeafModel internals
    doorLeaf.centerOffsetFromHinge = -pivot * leafPlacement.scale;  // approx
    doorLeaf.closedScale = leafPlacement.scale;
    doorLeaf.closedYaw = group.yawDegrees;
    doorLeaf.openYaw   = group.yawDegrees - group.openAngle;
    doorLeaf.colliderHalfExtents = ...;  // mesh-dependent

    // Door root entity
    auto doorRoot = builder.createTransformEntity(group.position + glm::vec3(0, 1, 0));
    registry.emplace<DoorComponent>(doorRoot, DoorComponent{leafEntity, entt::null, ...});
    registry.emplace<InteractableComponent>(doorRoot, ...);
    return doorRoot;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hardcoded hinge offset (-0.45, 0, 0.04) | `hingePivot` field on LevelSingleDoorPlacement | Phase quick 260406-uqb | Must migrate to `pivot` on LevelMeshPlacement |
| Scale hardcoded to 0.22 in builder | Scale hardcoded to 0.22 in builder | Never changed | After refactor, scale lives in mesh placement |
| Two pivot math copies (builder + preview) | Two pivot math copies (still present) | Never unified | Phase 19 must extract to shared helper |

**Deprecated/outdated:**
- `LevelSingleDoorPlacement`: replaced by `LevelDoorGroupPlacement` + child mesh placements
- `EditorSceneObjectKind::SingleDoor`: replaced by `EditorSceneObjectKind::DoorGroup`
- `EditorDoorLeafTag` marker: replaced by `LevelMeshPlacement::pivot.has_value()` check
- `spawnSingleDoor()` in GameplayPrefabs.cpp: replaced by `LevelBuilder::addDoorGroup()`
- `addSingleDoor()` in LevelBuilder.cpp: replaced by `addDoorGroup()`
- `SingleDoorInspector.cpp/.h`: replaced by `DoorGroupInspector.cpp/.h`
- `kDoorScale = 0.22f` constant in multiple files: removed, scale lives in mesh placements

## Open Questions

1. **centerOffsetFromHinge derivation**
   - What we know: Currently hardcoded as `(0.445f, 0, 0)` — this is half the SM_DoorA mesh X extent at scale 0.22. The formula should be `center = hinge + (mesh_origin - pivot) * scale`, and since mesh_origin is (0,0,0) local, `centerOffset = -pivot * scale`.
   - What's unclear: Is `pivot = (-0.45, 0, 0.04)` truly relative to mesh origin? The preview world math uses `T(hinge) * R(yaw) * T(-pivot) * S(scale)`, which means `pivot` is in pre-scale mesh-local space.
   - Recommendation: Verify empirically — place a door at origin with known pivot and compare rendered position vs `centerOffset`. If `centerOffset = -pivot * scale` matches the current 0.445 value (at scale 0.22, -(-0.45)*0.22 = 0.099 — this does NOT match 0.445), the formula is different. More likely the hinge offset is in world space, not scaled. Investigate before coding `addDoorGroup`.

2. **Collider half-extents source of truth**
   - What we know: Currently hardcoded as `(0.445f, 1.01f, 0.05f)` in `spawnSingleDoor`.
   - What's unclear: After refactor, where do these come from? Mesh bounds? Another field on the leaf mesh placement?
   - Recommendation: Keep them hardcoded per-mesh in `addDoorGroup` as a constant lookup, or add a `colliderHalfExtents` optional field on `LevelMeshPlacement`. The planner should decide; the simplest approach is to keep them in `addDoorGroup` per mesh ID.

3. **Migration strategy for initial_scene.scene**
   - What we know: 3 `single_door` records exist. Each needs to become a `door_group` + 2 `mesh` children.
   - What's unclear: Do we keep backward parsing support for `single_door` (old format read, new format written) or hard-cut?
   - Recommendation: Add a backward-compat parser that converts `single_door` records on load into the new group structure, writes them out as `door_group` on save. This avoids needing to manually edit all scene files and protects against format drift. Alternatively, do a one-time manual edit of the 3 entries (simpler, lower risk).

4. **Group click-to-select via children in viewport**
   - What we know: Currently `SingleDoor` maps two entities (frame + leaf) to the same owner ID in `ownerMap_`. This is how clicks on either entity select the door.
   - What's unclear: After refactor, child mesh objects have their own `EditorSceneObject` IDs. Clicking the frame mesh selects the frame (not the door group parent).
   - Recommendation: Implement click-bubbles-to-parent: when a child object is clicked, check if `parentObjectId` is a `DoorGroup`, and if so, select the parent instead. This matches the Decision: "Clicking any door part selects the parent Door group."

## Environment Availability

Step 2.6: SKIPPED (no external dependencies — pure C++ code refactor within existing toolchain)

## Sources

### Primary (HIGH confidence)
- `src/game/components/DoorComponent.h` — DoorComponent struct (leftLeaf, rightLeaf, progress)
- `src/game/components/DoorLeafComponent.h` — DoorLeafComponent (hingePosition, centerOffsetFromHinge, closedYaw, openYaw)
- `src/game/prefabs/GameplayPrefabs.cpp` — spawnSingleDoor full implementation
- `src/game/level/LevelDef.h` — LevelSingleDoorPlacement, LevelGroupNode, LevelDef
- `src/game/level/LevelDef.cpp` — serializer/parser for single_door, resolveLevelHierarchy
- `src/editor/scene/EditorSceneDocument.h/.cpp` — variant dispatch, addSingleDoor, applyWorldTransform, toLevelDef
- `src/editor/scene/EditorPreviewWorld.cpp` — rebuild and syncTransforms SingleDoor cases with EditorDoorLeafTag
- `src/editor/scene/EditorPreviewWorld.h` — EditorDoorLeafTag struct
- `src/editor/ui/inspectors/SingleDoorInspector.cpp` — all inspectable door fields
- `src/editor/viewport/EditorViewportInteraction.cpp` — SingleDoor cases in kind dispatch
- `.planning/quick/260406-vdu-fix-singledoor-architecture-to-properly-/260406-vdu-CONTEXT.md` — locked design decisions

### Secondary (MEDIUM confidence)
- `assets/scenes/initial_scene.scene` — 3 single_door records to migrate
- `src/game/behavior/DoorAnimationSystem.cpp` — runtime door animation (no changes needed)
- `src/game/runtime/RuntimeGameplay.cpp` — updateRuntimeDoors (no structural changes needed)

## Project Constraints (from CLAUDE.md)

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **ECS**: EnTT v3.16 — components are POD structs, no methods, no inheritance
- **Naming**: Classes/Structs PascalCase; functions/methods camelCase; private members trailing underscore
- **No scripted geometry**: Never define entities in code — they must come from `.scene` files
- **Shaders**: GLSL `#version 410 core` (macOS ceiling) — not applicable here but noted
- **Build**: CMake FetchContent; test registration via `pixel_roguelike_add_test()`
- **File naming**: `PascalCase.h` / `PascalCase.cpp` matching primary class name
- **Headers**: `#pragma once`, include order: stdlib → third-party → project
- **RAII**: OpenGL resources cleaned up in destructors — not directly applicable here
- **GSD Workflow**: All edits through GSD workflow commands; no direct repo edits outside GSD

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — entire refactor is within well-understood existing codebase; no new dependencies
- Architecture: HIGH — design decisions are locked in 260406-vdu-CONTEXT.md; all touch points catalogued
- Pitfalls: HIGH — each pitfall is derived from direct code inspection of the affected files
- Open Questions: MEDIUM — centerOffsetFromHinge derivation needs empirical verification before coding

**Research date:** 2026-04-06
**Valid until:** Stable (pure internal refactor, no external dependencies)
