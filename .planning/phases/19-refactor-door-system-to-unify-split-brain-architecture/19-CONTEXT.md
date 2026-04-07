# Phase 19: Refactor door system to unify split-brain architecture - Context

**Gathered:** 2026-04-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Eliminate the split-brain door architecture by unifying the two spawn paths (LevelDoorGroupPlacement + DoubleDoorSpawnSpec), two runtime paths (DoorAnimationSystem + RuntimeGameplay.cpp free functions), and five data representations into a single coherent system. Fix the right-leaf animation bug, separate config from state in ECS components, surface leaf/pivot info in the editor inspector, and clean up dead code and naming.

</domain>

<decisions>
## Implementation Decisions

### Unified Door Definition
- **D-01:** Always support dual leaves — unified struct has leftLeaf and rightLeaf fields. Single-leaf doors leave rightLeaf empty (entt::null). No variable-length leaf list.
- **D-02:** Hinge/pivot specification uses the pivot field on leaf meshes (current LevelDoorGroupPlacement approach). Hinge position derived from parent DoorGroup position + mesh pivot offset. makePivotLeafModel() remains the single source of truth for pivot math.
- **D-03:** Remove DoubleDoorSpawnSpec and the archetype/prefab spawn path entirely. Delete from GameplayPrefabData.h, GameplayPrefabs.cpp, ContentRegistry. All doors defined via unified struct in .scene files. One spawn path to maintain.
- **D-04:** Delete LevelDoorGroupPlacement. Replace with a new unified door placement struct in LevelDef.h that supports dual leaves with pivot-based hinges.

### Scene File Syntax
- **D-05:** Claude's discretion on whether to keep `door_group` keyword or rename to `door` — evaluate migration cost vs naming clarity per R6 (DoubleDoor -> Door cleanup).

### Config/State Split
- **D-06:** Split DoorComponent into DoorConfigComponent (spawn-time config) and DoorStateComponent (runtime animation state).
- **D-07:** DoorConfigComponent contains: leftLeaf, rightLeaf (entity refs), interactDistance, interactDotThreshold, openDuration, openAngle, locked, lockedPrompt. These are set once at spawn and never change.
- **D-08:** DoorStateComponent contains: progress (0..1), opening (bool), opened (bool), targetState (Open/Closed for bidirectional animation). Pure runtime state that resets between play sessions.
- **D-09:** Leaf entity references (leftLeaf, rightLeaf) belong in DoorConfigComponent — they're structural, set once at spawn time.
- **D-10:** Support bidirectional animation — DoorStateComponent tracks a target state (open/closed) and progress moves in both directions. CloseDoor/ToggleDoor actions play smooth reverse animation instead of snapping to closed.

### Editor Inspector UX
- **D-11:** DoorGroupInspector shows child leaf meshes as read-only display. Shows which meshes are leaves (left/right), their pivot values. Editing pivot requires selecting the leaf mesh directly in the outliner.
- **D-12:** When no leaf mesh children exist on a door group, display a yellow warning banner: "No leaf meshes found — this door won't animate."
- **D-13:** No quick-fix or auto-create leaf buttons — user manually adds child meshes with pivot fields.

### Runtime Unification
- **D-14:** Editor preview uses the real DoorAnimationSystem to tick doors during play preview. Guarantees visual parity with game runtime. System must work without full gameplay context.
- **D-15:** Delete ALL door-related free functions from RuntimeGameplay.cpp — updateRuntimeDoorAnimation() and any door helpers. DoorAnimationSystem is the single source of truth for door animation in both game and editor.
- **D-16:** Fix right-leaf animation bug as part of this unification — both leaves animate correctly via DoorAnimationSystem.

### Dead Code and Naming Cleanup
- **D-17:** Delete computeHingeWorldPos() from GameplayPrefabs.cpp/h — it's dead code that just returns basePos. Editor pivot visualization uses DoorGroup position directly.
- **D-18:** Rename DoubleDoor references to Door where appropriate per R6. Exact scope is Claude's discretion based on what files are touched by the refactor.

### Claude's Discretion
- Scene file keyword choice (door_group vs door) and migration strategy
- Internal structure of the unified door placement struct (field names, defaults)
- How DoorAnimationSystem detects editor vs game context for preview support
- Whether DoorLeafComponent needs any changes or stays as-is
- Exact naming for DoorConfigComponent/DoorStateComponent (may differ slightly if better names emerge)
- How to handle DoorActionParams after the refactor

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Door spawn paths (being unified)
- `src/game/level/LevelDef.h` — LevelDoorGroupPlacement struct (lines 111-123), LevelDef aggregate
- `src/game/level/LevelDef.cpp` — door_group parser (lines 971-1010), serializer
- `src/game/level/LevelBuilder.cpp` — addDoorGroup() spawn function (lines 282-423)
- `src/game/prefabs/GameplayPrefabData.h` — DoubleDoorSpawnSpec struct (lines 23-35, being deleted)
- `src/game/prefabs/GameplayPrefabs.cpp` — spawnDoubleDoor() (lines 130-188, being deleted), computeHingeWorldPos (lines 89-95, dead code)
- `src/game/prefabs/GameplayPrefabs.h` — spawnDoubleDoor declarations, computeHingeWorldPos declaration

### Door ECS components (being split/refactored)
- `src/game/components/DoorComponent.h` — Current DoorComponent (being split into Config + State)
- `src/game/components/DoorLeafComponent.h` — DoorLeafComponent (per-leaf geometry data)

### Door animation system
- `src/game/behavior/DoorAnimationSystem.cpp` — updateDoorLeaf() (lines 18-44), init/update (lines 46-110)
- `src/game/behavior/DoorAnimationSystem.h` — System declaration
- `src/game/behavior/ActionTypes.h` — DoorActionParams (lines 26-28)
- `src/game/runtime/RuntimeGameplay.cpp` — updateRuntimeDoorAnimation() (lines 486-533, being deleted)

### Behavior system (door activation)
- `src/game/behavior/BehaviorSystem.cpp` — Door action dispatch (OpenDoor, CloseDoor, ToggleDoor)
- `src/game/behavior/BehaviorComponent.h` — BehaviorComponent action lists

### Editor door handling
- `src/editor/ui/inspectors/DoorGroupInspector.cpp` — Current inspector (lines 12-117, being extended)
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Pivot visualization (lines 305-348)
- `src/editor/scene/EditorPreviewWorld.cpp` — Preview door rebuild (lines 176-196), syncTransforms (lines 346-368)
- `src/editor/scene/EditorSceneDocument.h` — EditorSceneObjectKind::DoorGroup

### Scene files to migrate
- `assets/scenes/initial_scene.scene` — Primary scene with doors
- `assets/scenes/country_house.scene` — Scene with doors
- `assets/prefabs/*.prefab` — Any prefab files referencing DoubleDoorSpawnSpec

### Debug context
- `.planning/debug/door-leaf-gizmo-moves-pivot.md` — Prior door gizmo bug fix
- `.planning/debug/door-edit-play-position-mismatch.md` — Prior edit/play position mismatch fix

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `makePivotLeafModel()` — Core pivot math function already shared between DoorAnimationSystem and editor preview. The unified system centers on this function.
- `DoorLeafComponent` — Per-leaf geometry data. Already contains basePosition, pivot, meshCenter, closedScale, closedYaw, openYaw. May not need changes.
- `DoorGroupInspector.cpp` — Existing inspector with position, yaw, behavior, and lock sections. Extend rather than rewrite.

### Established Patterns
- Phase 17 unified colliders: ColliderComponent replaced both StaticColliderComponent and TriggerComponent. Same pattern applies here — unified DoorConfigComponent replaces both spawn path structs.
- Phase 18 inspector decomposition: Per-type inspector files with thin dispatcher. DoorGroupInspector already follows this pattern.
- Phase 13 behavior system: BehaviorSystem handles activation dispatch, animation systems handle per-frame updates. This separation is maintained.

### Integration Points
- `LevelBuilder.cpp` — addDoorGroup() is the primary spawn entry point. Must be updated for unified struct.
- `BehaviorSystem.cpp` — Dispatches OpenDoor/CloseDoor/ToggleDoor actions. Must read new DoorConfigComponent + DoorStateComponent.
- `EditorPreviewWorld.cpp` — Must instantiate DoorAnimationSystem for play preview instead of direct makePivotLeafModel() calls.

</code_context>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches within the decisions above.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 19-refactor-door-system-to-unify-split-brain-architecture*
*Context gathered: 2026-04-07*
