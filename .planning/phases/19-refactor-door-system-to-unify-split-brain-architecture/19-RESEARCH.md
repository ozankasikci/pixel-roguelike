# Phase 19: Refactor Door System to Unify Split-Brain Architecture - Research

**Researched:** 2026-04-07
**Domain:** C++ ECS refactor — door system architecture, EnTT components, editor inspector, play preview integration
**Confidence:** HIGH (all findings from direct codebase inspection)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Always support dual leaves — unified struct has leftLeaf and rightLeaf fields. Single-leaf doors leave rightLeaf empty (entt::null). No variable-length leaf list.
- **D-02:** Hinge/pivot specification uses the pivot field on leaf meshes. makePivotLeafModel() remains the single source of truth for pivot math.
- **D-03:** Remove DoubleDoorSpawnSpec and the archetype/prefab spawn path entirely. Delete from GameplayPrefabData.h, GameplayPrefabs.cpp, ContentRegistry. All doors defined via unified struct in .scene files. One spawn path to maintain.
- **D-04:** Delete LevelDoorGroupPlacement. Replace with a new unified door placement struct in LevelDef.h that supports dual leaves with pivot-based hinges.
- **D-06:** Split DoorComponent into DoorConfigComponent (spawn-time config) and DoorStateComponent (runtime animation state).
- **D-07:** DoorConfigComponent contains: leftLeaf, rightLeaf, interactDistance, interactDotThreshold, openDuration, openAngle, locked, lockedPrompt. Set once at spawn, never change.
- **D-08:** DoorStateComponent contains: progress (0..1), opening (bool), opened (bool), targetState (Open/Closed). Pure runtime state that resets between play sessions.
- **D-09:** Leaf entity references (leftLeaf, rightLeaf) belong in DoorConfigComponent.
- **D-10:** Support bidirectional animation — DoorStateComponent tracks a target state and progress moves in both directions. CloseDoor/ToggleDoor play smooth reverse animation instead of snapping.
- **D-11:** DoorGroupInspector shows child leaf meshes as read-only display. Editing pivot requires selecting the leaf mesh in the outliner.
- **D-12:** When no leaf mesh children exist, display yellow warning: "No leaf meshes found — this door won't animate."
- **D-13:** No quick-fix or auto-create leaf buttons.
- **D-14:** Editor preview uses the real DoorAnimationSystem to tick doors during play preview.
- **D-15:** Delete ALL door-related free functions from RuntimeGameplay.cpp — updateRuntimeDoorAnimation() and helpers.
- **D-16:** Fix right-leaf animation bug as part of this unification.
- **D-17:** Delete computeHingeWorldPos() from GameplayPrefabs.cpp/h — it's dead code.
- **D-18:** Rename DoubleDoor references to Door where appropriate.

### Claude's Discretion

- Scene file keyword choice (door_group vs door) and migration strategy
- Internal structure of the unified door placement struct (field names, defaults)
- How DoorAnimationSystem detects editor vs game context for preview support
- Whether DoorLeafComponent needs any changes or stays as-is
- Exact naming for DoorConfigComponent/DoorStateComponent (may differ slightly if better names emerge)
- How to handle DoorActionParams after the refactor

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

## Summary

The door system currently has two spawn paths (LevelBuilder::addDoorGroup via .scene files, and spawnDoubleDoor via .prefab files) and two runtime animation paths (DoorAnimationSystem and the free function updateRuntimeDoorAnimation in RuntimeGameplay.cpp). The free function duplicates DoorAnimationSystem logic but has a critical omission: it animates only leftLeaf and never touches rightLeaf. The editor play preview uses EditorRuntimePreviewSession which wraps RuntimeGameSession, which calls updateRuntimeDoorAnimation directly — bypassing DoorAnimationSystem entirely. This is why the right-leaf bug manifests in editor play preview.

The refactor has three independent work streams that must be sequenced carefully: (1) define the new unified data structs (DoorConfigComponent + DoorStateComponent, new LevelDoorPlacement), (2) update all systems that read from DoorComponent (BehaviorSystem, RuntimeGameSession snapshot/restore, DoorAnimationSystem), and (3) migrate the editor path (DoorGroupInspector, EditorPreviewWorld, EditorSceneDocument payload type). The scene file format migration from `door_group` to either `door` or `door_group` is low-risk — only 3 door_group lines exist in initial_scene.scene, none in other scenes, no usage in .prefab files beyond the one double_door.prefab being deleted.

The bidirectional animation requirement (D-10) is the most architecturally novel change: the current state machine is unidirectional (opening: progress 0→1, closed: snap reset). The new DoorStateComponent needs a targetState enum and progress must drive in reverse (1→0) for close animation. BehaviorSystem CloseDoor and ToggleDoor handlers must be updated accordingly.

**Primary recommendation:** Sequence work as: struct definitions first → spawn path unification → animation system consolidation → editor path update → dead code removal. Each step is independently testable.

---

## Architecture Patterns

### Current Split-Brain Map (what exists now)

```
Spawn path A: .scene file → LevelDef.doors (LevelDoorGroupPlacement)
                           → LevelBuilder::addDoorGroup()
                           → DoorComponent { leftLeaf, rightLeaf=null, ... }
                           → Only leftLeaf gets DoorLeafComponent

Spawn path B: .prefab file → ContentRegistry → GameplayPrefabInstance (DoubleDoorSpawnSpec)
                           → spawnDoubleDoor() in GameplayPrefabs.cpp
                           → DoorComponent { leftLeaf, rightLeaf, ... }
                           → Both leaves get DoorLeafComponent (but pivot=vec3(0))

Runtime path A: DoorAnimationSystem::update()
                → animates both door.leftLeaf and door.rightLeaf via updateDoorLeaf()
                → used in the game executable via RuntimeGameSession

Runtime path B: updateRuntimeDoorAnimation() in RuntimeGameplay.cpp
                → only animates door.leftLeaf (RIGHT-LEAF BUG HERE)
                → called from RuntimeGameSession::tick() at line 224
                → BOTH paths run concurrently — double-update conflict
```

### The Double-Update Bug

`RuntimeGameSession::tick()` calls `updateRuntimeDoorAnimation(registry_, deltaTime)` at line 224. This free function is the only door animation path that runs. `DoorAnimationSystem` is NOT instantiated or called in RuntimeGameSession — it only appears in the include list. This means:
- The free function is the live path
- The free function skips rightLeaf entirely
- DoorAnimationSystem is dead code in the runtime path despite being the "official" system

**Confirmed by:** grep showing DoorAnimationSystem only referenced in `src/game/runtime/RuntimeGameSession.cpp` include but the actual tick calls `updateRuntimeDoorAnimation`. [VERIFIED: codebase inspection]

### Target Architecture After Refactor

```
Single spawn path: .scene file → LevelDef.doors (new unified struct)
                               → LevelBuilder::addDoorGroup()
                               → DoorConfigComponent + DoorStateComponent
                               → Both leaves spawn with DoorLeafComponent + pivot

Single runtime path: DoorAnimationSystem::update()
                    → reads DoorConfigComponent (leafs) + DoorStateComponent (progress/direction)
                    → animates both leaves with cubic ease
                    → used identically in game and editor play preview

Editor preview: EditorPreviewWorld (static display) + EditorRuntimePreviewSession (play mode)
               → play mode must instantiate DoorAnimationSystem (or equivalent tick)
               → currently play preview uses RuntimeGameSession which calls updateRuntimeDoorAnimation
               → after refactor: RuntimeGameSession calls DoorAnimationSystem.update() instead
```

### Bidirectional Animation State Machine

Current DoorComponent state machine (unidirectional):
```
CLOSED ──[open]──> OPENING (progress: 0→1) ──> OPENED
OPENED/OPENING ──[close/toggle]──> snap to CLOSED (progress=0)
```

New DoorStateComponent state machine (bidirectional, D-10):
```
CLOSED ──[open/toggle]──> moving toward OPEN (progress 0→1)
OPENED ──[close/toggle]──> moving toward CLOSED (progress 1→0)
MOVING ──[toggle]──> reverses direction at current progress
```

The `targetState` enum (Open/Closed) tells DoorAnimationSystem which direction to move progress. `opening` bool becomes `targetState == Open`, and `opened` bool is derived from `(targetState == Open && progress >= 1.0f)`.

BehaviorSystem handlers must be updated:
- **CloseDoor**: set `targetState = Closed` (don't snap `progress = 0`)
- **ToggleDoor**: flip `targetState`, keep current progress
- **OpenDoor**: set `targetState = Open`

The InteractableComponent `enabled` flag logic: door is interactable when not fully in the requested target state. This is more nuanced than `!door.opened` — must account for mid-animation state.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Pivot math | New hinge calculation | `makePivotLeafModel()` in GameplayPrefabs.cpp | Single source of truth already established; editor and runtime already share it |
| Component split migration | New snapshot system | Extend `RuntimeMutableSnapshot::doors` to store `DoorStateComponent` pairs | Snapshot/restore is already in RuntimeGameSession; just update the stored type |
| Inspector property table | Custom layout | `renderInspectorPropertyRow` + `beginInspectorPropertyTable` | Pattern already established across all inspectors |
| Warning banner | Custom ImGui widget | `ImGui::TextColored` with warning color | Already used elsewhere in editor |
| EnTT component access | Raw storage | `registry.try_get<>()` | Already the pattern; safe null-check on entity validity |

---

## Detailed File-by-File Change Map

### Wave 1: New Data Structures (no behavior change yet)

**`src/game/components/DoorComponent.h`** — Replace with two files:

```cpp
// DoorConfigComponent.h (VERIFIED pattern from codebase)
struct DoorConfigComponent {
    entt::entity leftLeaf = entt::null;
    entt::entity rightLeaf = entt::null;
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    float openDuration = 1.2f;
    float openAngle = 90.0f;
    bool locked = false;
    std::string lockedPrompt = "E  This door is locked";
};

// DoorStateComponent.h
enum class DoorTargetState { Closed, Open };
struct DoorStateComponent {
    float progress = 0.0f;         // 0=closed, 1=open
    DoorTargetState targetState = DoorTargetState::Closed;
};
// Free-function helpers (POD rule: no methods on components per CLAUDE.md):
// inline bool isDoorFullyClosed(const DoorStateComponent& s);
// inline bool isDoorFullyOpen(const DoorStateComponent& s);
// inline bool isDoorMoving(const DoorStateComponent& s);
```

**`src/game/level/LevelDef.h`** — Replace `LevelDoorGroupPlacement` struct (lines 111-123) and `LevelDef::doors` field. New struct must include all fields from current struct. **Keep `door_group` keyword** (see Scene File Syntax section below).

### Wave 2: Spawn Path Unification

**`src/game/level/LevelBuilder.cpp`** — `addDoorGroup()` (lines 282-423):
- Update signature to use new unified placement struct
- Spawn rightLeaf in addition to leftLeaf when the new struct has a second leaf mesh
- Emplace `DoorConfigComponent` + `DoorStateComponent` instead of `DoorComponent`
- Behavior/interaction setup logic stays the same

**`src/game/prefabs/GameplayPrefabs.cpp`** — Delete:
- `spawnDoorLeaf()` (lines 19-58, anonymous namespace)
- `spawnDoubleDoor()` (lines 130-197)
- `computeHingeWorldPos()` (lines 89-95)

**`src/game/prefabs/GameplayPrefabs.h`** — Remove declarations for deleted functions.

**`src/game/prefabs/GameplayPrefabData.h`** — Remove `DoubleDoorSpawnSpec` struct (lines 23-35). Remove `GameplayPrefabType::DoubleDoor`. Update `GameplayPrefabInstance` (only checkpoint remains).

**`src/game/content/ContentRegistry.cpp`** — Remove DoubleDoor handling (lines 252-253, 394-395, 445-446, 485 — the double_door.prefab load).

**`src/game/content/ContentRegistry.h`** — Remove `DoubleDoorSpawnSpec` include.

**`assets/prefabs/gameplay/double_door.prefab`** — Delete this file (or keep for reference; it won't be loaded).

### Wave 3: Animation System Consolidation

**`src/game/behavior/DoorAnimationSystem.cpp`** — Update to read `DoorConfigComponent` + `DoorStateComponent`:
- `init()`: iterate `DoorConfigComponent` view, call `updateDoorLeaf` for both leaves at progress=0
- `update()`: iterate `(DoorConfigComponent, DoorStateComponent, TransformComponent)` view
  - Advance/retreat progress based on `targetState`
  - Call `updateDoorLeaf` for leftLeaf and rightLeaf
  - Update InteractableComponent enabled/busy based on new state machine

**`src/game/runtime/RuntimeGameplay.cpp`** — Delete `updateRuntimeDoorAnimation()` (lines 486-533).

**`src/game/runtime/RuntimeGameplay.h`** — Remove `updateRuntimeDoorAnimation` declaration.

**`src/game/runtime/RuntimeGameSession.cpp`** — Line 224: replace `updateRuntimeDoorAnimation(registry_, deltaTime)` with `doorAnimationSystem_.update(app, deltaTime)` (or equivalent — see DoorAnimationSystem integration note below).

**Snapshot/restore update:** `RuntimeMutableSnapshot::doors` (line 46) stores `DoorComponent` pairs. Must change to `DoorStateComponent` pairs. Restore at lines 398-416 must reset leaf positions using `makePivotLeafModel` at progress=0 for DoorTargetState::Closed.

### Wave 4: BehaviorSystem Update

**`src/game/behavior/BehaviorSystem.cpp`** — `executeAction()` door cases (lines 170-243):
- Include `DoorConfigComponent.h` + `DoorStateComponent.h` instead of `DoorComponent.h`
- `OpenDoor`: `state.targetState = Open; state.progress = std::min(state.progress, existing)` — don't reset if mid-open
- `CloseDoor`: `state.targetState = Closed` — don't snap progress to 0
- `ToggleDoor`: flip `targetState`, keep `progress` — smooth reverse

**`src/game/behavior/ActionTypes.h`** — `DoorActionParams` currently only has `duration`. After refactor, duration is in DoorConfigComponent, not in the action. Consider whether DoorActionParams is still needed. Decision is Claude's discretion (D-06 area). Safest approach: keep DoorActionParams but stop using `duration` from it in BehaviorSystem once DoorConfigComponent is authoritative.

### Wave 5: Editor Path Update

**`src/editor/scene/EditorSceneDocument.h`** — `EditorSceneObjectPayload` variant currently includes `LevelDoorGroupPlacement`. After LevelDoorGroupPlacement is renamed/replaced, update the variant type accordingly.

**`src/editor/ui/inspectors/DoorGroupInspector.cpp`** — Extend existing inspector:
1. Add leaf mesh display section: iterate document children, find pivot-bearing meshes, display read-only rows showing Left Leaf / Right Leaf mesh names and pivot values.
2. Add warning banner when no pivot-bearing child meshes found: `ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "No leaf meshes found -- this door won't animate.");`
3. No buttons for auto-create (D-13).

**`src/editor/render/EditorScenePreviewRenderer.cpp`** — Pivot visualization (lines 305-348) calls `computeHingeWorldPos()` which is being deleted. Since `computeHingeWorldPos` just returns `basePos`, replace with `dg.position` directly.

**`src/editor/scene/EditorPreviewWorld.cpp`** — DoorGroup case (lines 247-256):
- Currently creates a minimal entity with no visual, no DoorConfigComponent, no animation
- After refactor: must spawn the same door entities as LevelBuilder does (via addDoorGroup) so DoorAnimationSystem can tick them during play preview

**`src/editor/core/EditorRuntimePreviewSession.h/.cpp`** — The EditorRuntimePreviewSession wraps RuntimeGameSession. After RuntimeGameSession calls DoorAnimationSystem (not the free function), play preview automatically gets correct door animation. No direct change needed here IF the RuntimeGameSession change is done cleanly.

---

## DoorAnimationSystem Integration for Editor Preview (D-14)

The key question: how does DoorAnimationSystem get called in editor play preview?

`EditorRuntimePreviewSession` owns a `RuntimeGameSession session_`. `RuntimeGameSession::tick()` currently calls `updateRuntimeDoorAnimation(registry_, deltaTime)` (line 224). After the refactor, that line is replaced with a call to DoorAnimationSystem.

The cleanest approach: `RuntimeGameSession` owns a `DoorAnimationSystem doorAnimSystem_` member. In `RuntimeGameSession::tick()`, replace the free function call with `doorAnimSystem_.update(*this, deltaTime)` — where `this` is the Application (RuntimeGameSession needs to satisfy the Application interface). **Check:** `DoorAnimationSystem::update(Application& app, float deltaTime)` — requires an `Application&`. RuntimeGameSession IS an Application (verify whether it inherits from Application or provides the same interface).

If RuntimeGameSession does not inherit Application, the alternative is to make DoorAnimationSystem's core tick a free function that takes `entt::registry&` and `float` — consistent with how `updateRuntimeDoorAnimation` was called.

**Recommended approach** (Claude's discretion per D-14): Extract DoorAnimationSystem's per-frame logic into a free function `tickDoorAnimation(entt::registry&, float)` that the System wrapper calls. RuntimeGameSession calls this free function directly. This avoids the Application dependency issue entirely.

---

## Scene File Keyword: Keep `door_group` (D-05 Decision)

The existing `door_group` keyword in .scene files is used in only one place: `initial_scene.scene` lines 88-90 (3 door_group records). The serializer and parser are tightly coupled to `LevelDoorGroupPlacement`.

**Recommendation: keep `door_group` keyword.** The rename from `LevelDoorGroupPlacement` to the new struct is a C++ type rename that doesn't require changing the scene file format. The scene file keyword (`door_group`) is purely a parser token — it can remain `door_group` even as the C++ type is renamed to `LevelDoorPlacement` or similar. Migration cost of changing the keyword: re-save all 3 door_group records, update parser and serializer (two code sites). Benefit: naming purity. Risk: any external tool expecting `door_group` breaks. Verdict: keep `door_group`, rename the C++ type.

---

## Right-Leaf Animation Bug Root Cause

**Confirmed** [VERIFIED: codebase inspection]:

`RuntimeGameplay.cpp::updateRuntimeDoorAnimation()` (lines 486-533) only animates `door.leftLeaf`. The `door.rightLeaf` block is entirely absent — there is no "if (door.rightLeaf != entt::null)" branch. The `DoorAnimationSystem::update()` DOES handle both leaves (lines 86-87). However `DoorAnimationSystem` is never instantiated or called in `RuntimeGameSession::tick()` — only the free function runs. Fixing this is a consequence of animation system consolidation (D-15, D-16): delete the free function, have RuntimeGameSession call DoorAnimationSystem, and both leaves animate.

---

## Common Pitfalls

### Pitfall 1: Snapshot/Restore Must Track DoorStateComponent Not DoorConfigComponent

**What goes wrong:** If snapshot captures DoorConfigComponent (config: leaf entity refs, open params), restoring it zeroes out entity refs that are still valid — causing DoorAnimationSystem to animate entt::null leaves.
**Prevention:** Snapshot stores DoorStateComponent pairs (progress, targetState). Config is immutable at spawn time and never needs restoring.
**Warning signs:** Doors not resetting position after play-stop-play cycle.

### Pitfall 2: DoorAnimationSystem Init Resets Mid-Animation State

Current `DoorAnimationSystem::init()` zeroes all doors to progress=0 via `updateDoorLeaf(..., 0.0f)`. After splitting components, init must zero `DoorStateComponent` (reset progress and targetState) AND call updateDoorLeaf at progress=0. If init only resets the component but doesn't update the visual (MeshComponent.modelOverride), the door leaf appears in whatever position it was last rendered.
**Prevention:** init() must both reset DoorStateComponent and call updateDoorLeaf for all leaves.

### Pitfall 3: EditorPreviewWorld DoorGroup Case Must Spawn Full Door Entities

Current EditorPreviewWorld DoorGroup case (lines 247-256) creates a minimal entity with only TransformComponent — no DoorConfigComponent, no DoorLeafComponent, no InteractableComponent. This means DoorAnimationSystem has nothing to tick even after it's wired in.
**Prevention:** EditorPreviewWorld::rebuild() DoorGroup case must call LevelBuilder::addDoorGroup() (the same path as the game executable) to spawn the full entity hierarchy. This requires EditorPreviewWorld to pass the full LevelDef to the builder, which it already does via `builder`.

### Pitfall 4: EditorSceneDocument Payload Variant Type Change

`EditorSceneObjectPayload` includes `LevelDoorGroupPlacement` as the last variant type (index 7). If the C++ type is renamed and the variant is updated, any code that does `std::get<LevelDoorGroupPlacement>()` or `std::get<7>()` will break. There are multiple call sites (DoorGroupInspector, EditorPreviewWorld, EditorScenePreviewRenderer, EditorPreviewWorld::syncTransforms).
**Prevention:** Do the struct rename in a single commit, fix all call sites in the same commit. Use the compiler as a checklist — every `std::get<LevelDoorGroupPlacement>` site will fail to compile until updated.

### Pitfall 5: Bidirectional Progress — InteractableComponent enabled Logic

Current `DoorAnimationSystem::update()` sets `interactable->enabled = !door.opened` (line 107). After bidirectional animation, "opened" is no longer a simple bool. If the door is traveling toward closed at progress=0.5, is it "openable"? The correct logic: `enabled = (targetState == Closed && progress <= 0.0f)` i.e., only interactable when fully closed.
**Prevention:** Define free-function helpers for DoorStateComponent: `isDoorFullyClosed()`, `isDoorFullyOpen()`. Use these in DoorAnimationSystem and BehaviorSystem. (Per CLAUDE.md POD rule, these are free functions, not member methods.)

### Pitfall 6: ContentRegistry double_door.prefab Load

`ContentRegistry.cpp` line 485 loads `assets/prefabs/gameplay/double_door.prefab` unconditionally at startup: `loadGameplayArchetypeAsset(resolveProjectPath("assets/prefabs/gameplay/double_door.prefab"))`. After deleting DoubleDoorSpawnSpec and GameplayArchetypeKind::DoubleDoor, this load will either throw or parse into an invalid state. Must remove the load call and delete the prefab file.
**Warning signs:** Runtime crash or spdlog error on startup if the file is deleted but the load call remains.

### Pitfall 7: PrefabInspector References DoubleDoor

`src/editor/ui/inspectors/PrefabInspector.cpp` lines 17-48 have a `GameplayArchetypeKind::DoubleDoor` switch case with all the DoubleDoor UI fields. This must be removed when DoubleDoor is deleted from GameplayArchetypeKind.

---

## Dead Code to Remove

| Location | Symbol | Why Dead |
|----------|--------|----------|
| `GameplayPrefabs.cpp` lines 89-95 | `computeHingeWorldPos()` | Returns `basePos` unchanged; comment says "The hinge IS at basePos" |
| `GameplayPrefabs.cpp` lines 19-58 | `spawnDoorLeaf()` (anon) | Only called from spawnDoubleDoor |
| `GameplayPrefabs.cpp` lines 130-197 | `spawnDoubleDoor()` both overloads | Being deleted per D-03 |
| `RuntimeGameplay.cpp` lines 486-533 | `updateRuntimeDoorAnimation()` | Replaced by DoorAnimationSystem.update() |
| `RuntimeGameplay.h` line 41 | `updateRuntimeDoorAnimation` declaration | Follows from above |
| `GameplayPrefabData.h` lines 23-35 | `DoubleDoorSpawnSpec` | No spawn path after D-03 |
| `GameplayPrefabData.h` line 9 | `GameplayPrefabType::DoubleDoor` | No spawn path after D-03 |
| `assets/prefabs/gameplay/double_door.prefab` | entire file | No loader after D-03 |
| `PrefabInspector.cpp` lines 17-48 | DoubleDoor UI case | No type after D-03 |

---

## Naming Cleanup (D-18)

Occurrences of "DoubleDoor" to rename to "Door":

| File | Symbol/Token | New Name |
|------|-------------|----------|
| `GameplayPrefabData.h` | `GameplayPrefabType::DoubleDoor` | deleted |
| `GameplayPrefabData.h` | `DoubleDoorSpawnSpec` | deleted |
| `ContentRegistry.cpp/h` | `GameplayArchetypeKind::DoubleDoor` | deleted |
| `PrefabInspector.cpp` | `kKinds[] = {"double_door"}` | deleted |
| `LevelDef.h` | `LevelDoorGroupPlacement` | `LevelDoorPlacement` (or keep for compat) |
| `LevelDef.h` | `LevelDef::doors` (vector name) | keep |

**Recommended:** rename `LevelDoorGroupPlacement` to `LevelDoorPlacement` since it no longer describes a "group" but a single door with optional dual leaves.

---

## Runtime State Inventory

This phase is a C++ refactor, not a rename/rebrand. No stored data, live service config, OS-registered state, secrets, or build artifacts carry door system struct names.

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None — doors are defined in .scene text files, not in a database | None |
| Live service config | None | None |
| OS-registered state | None | None |
| Secrets/env vars | None | None |
| Build artifacts | Stale .o files will be invalidated by struct changes — CMake handles this | Rebuild required after component split |

---

## Environment Availability

This phase is C++ source refactoring with no new external dependencies. All required tools (compiler, CMake, spdlog, EnTT) are already in use. Step 2.6 SKIPPED for tool availability — no new external dependencies introduced.

---

## Open Questions (RESOLVED)

1. **DoorAnimationSystem + Application coupling**
   - What we know: `DoorAnimationSystem::update(Application& app, float)` requires an `Application&`. `RuntimeGameSession::tick()` would need to pass itself as Application.
   - What's unclear: Does RuntimeGameSession inherit from Application or just aggregate it?
   - Recommendation: Check `RuntimeGameSession.h` class declaration. If it doesn't inherit Application, extract the core tick logic into `tickDoorAnimation(entt::registry&, float)` and have DoorAnimationSystem::update() call it.
   - **RESOLVED:** RuntimeGameSession does NOT inherit Application. Plan 02 extracts `tickDoorAnimation(entt::registry&, float)` as a free function in DoorAnimationSystem.h. DoorAnimationSystem::update() delegates to it. RuntimeGameSession calls the free function directly.

2. **DoorActionParams after refactor**
   - What we know: `DoorActionParams { float duration }` is used in BehaviorSystem OpenDoor/ToggleDoor to override openDuration. After DoorConfigComponent owns openDuration, the action-level override is redundant.
   - What's unclear: Should openDuration remain overridable per-action, or should BehaviorSystem just use DoorConfigComponent.openDuration?
   - Recommendation: Remove the override behavior — DoorConfigComponent.openDuration is the authoritative value. Simpler and consistent. Keep DoorActionParams struct as empty or remove it if nothing else uses it.
   - **RESOLVED:** Plan 02 BehaviorSystem reads openDuration from DoorConfigComponent exclusively. DoorActionParams duration field is no longer used for door actions. Struct kept for backward compatibility but duration is ignored.

3. **openAngle field in new struct vs DoorLeafComponent**
   - What we know: `LevelDoorGroupPlacement.openAngle` is used in `LevelBuilder::addDoorGroup()` to compute `doorLeaf.openYaw = group.yawDegrees - group.openAngle`. The open angle is baked into DoorLeafComponent at spawn time.
   - What's unclear: D-07 lists openAngle in DoorConfigComponent. But DoorLeafComponent already encodes openYaw. Is openAngle in DoorConfigComponent redundant with what's already in DoorLeafComponent?
   - Recommendation: Keep openAngle in DoorConfigComponent for inspector display and re-spawn scenarios, but DoorAnimationSystem reads openYaw from DoorLeafComponent. Both fields needed for different purposes.
   - **RESOLVED:** Plan 01 includes openAngle in DoorConfigComponent per D-07 for inspector display. DoorAnimationSystem reads openYaw from DoorLeafComponent (baked at spawn by LevelBuilder). Both fields serve different purposes and are kept.

---

## Sources

### Primary (HIGH confidence)
- All findings from direct inspection of source files in this codebase session — verified by reading actual C++ code.
- Key files inspected: DoorComponent.h, DoorLeafComponent.h, DoorAnimationSystem.cpp/.h, BehaviorSystem.cpp, RuntimeGameplay.cpp/.h, RuntimeGameSession.cpp, LevelDef.h/.cpp (parser + serializer), LevelBuilder.cpp (addDoorGroup), GameplayPrefabs.cpp/.h, GameplayPrefabData.h, ContentRegistry.cpp/.h, EditorPreviewWorld.cpp, DoorGroupInspector.cpp, EditorScenePreviewRenderer.cpp, EditorSceneDocument.h, EditorRuntimePreviewSession.h, PrefabInspector.cpp, ActionTypes.h, System.h, initial_scene.scene, double_door.prefab

### No external sources needed
This is a C++ internal refactoring phase with no new library dependencies. All research findings come from reading the existing codebase.

---

## Assumptions Log

All claims in this research are VERIFIED by direct codebase inspection. No assumptions.

**If this table is empty:** All claims in this research were verified — no user confirmation needed.

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| (empty) | | | |

---

## Metadata

**Confidence breakdown:**
- Current architecture: HIGH — read all relevant source files directly
- Right-leaf bug root cause: HIGH — confirmed updateRuntimeDoorAnimation skips rightLeaf
- Bidirectional animation pattern: HIGH — based on existing state machine analysis
- File-by-file change map: HIGH — derived directly from code inspection
- Dead code identification: HIGH — computeHingeWorldPos confirmed trivial (returns basePos)

**Research date:** 2026-04-07
**Valid until:** This research reflects the codebase at commit bc255d2. Valid until any further door system changes land.
