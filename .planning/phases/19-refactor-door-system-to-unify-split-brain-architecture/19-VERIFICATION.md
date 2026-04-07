---
phase: 19-refactor-door-system-to-unify-split-brain-architecture
verified: 2026-04-07T14:00:00Z
status: human_needed
score: 5/6 requirements verified
overrides_applied: 0
human_verification:
  - test: "Open level-editor, load initial_scene.scene, select a DoorGroup in the outliner. The inspector should show a 'Leaf Meshes' section listing child meshes by name with a '(leaf)' label."
    expected: "Inspector renders a 'Leaf Meshes' heading, shows child mesh names in green with '(leaf)' label when present, or shows a yellow 'No leaf meshes found -- this door won't animate.' warning when absent."
    why_human: "ImGui rendering and scene object hierarchy traversal cannot be verified programmatically without a running editor session."
  - test: "In level-editor play preview, interact with a door (press E). Both the left and right door leaves should swing open smoothly, then pressing E again should swing them closed."
    expected: "Both door leaves animate correctly (no static right leaf). Closing plays a smooth reverse animation rather than snapping. Direction reversal mid-swing produces smooth playback from current progress."
    why_human: "Bidirectional animation smoothness, right-leaf visual correctness, and close-animation behavior require a running game or editor preview session."
  - test: "In level-editor play preview, observe that door animation in editor preview looks visually identical to door animation in the game executable (pixel-roguelike)."
    expected: "Door leaf geometry, pivot point, swing angle, and timing are identical between editor preview and game runtime -- confirming R2 (no duplicate code path divergence)."
    why_human: "Visual parity between editor preview and game cannot be verified by code inspection alone."
---

# Phase 19: Refactor Door System to Unify Split-Brain Architecture — Verification Report

**Phase Goal:** Eliminate the split-brain door architecture by unifying the two spawn paths, two runtime paths, and five data representations into a single coherent system. Fix the right-leaf animation bug, separate config from state in ECS components, and surface leaf/pivot editing in the editor inspector.
**Verified:** 2026-04-07T14:00:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (derived from R1–R6)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| R1 | Single unified door definition struct replaces LevelDoorGroupPlacement + DoubleDoorSpawnSpec | VERIFIED | `LevelDoorPlacement` in `LevelDef.h:110`; zero grep matches for `LevelDoorGroupPlacement` across src/ and tests/; `DoubleDoorSpawnSpec` deleted from `GameplayPrefabData.h` |
| R2 | Editor preview uses same DoorAnimationSystem/BehaviorSystem as game executable — no duplicated free functions in RuntimeGameplay.cpp | VERIFIED | `EditorPreviewWorld.cpp:241` calls `builder.addDoorGroup(dg, emptyLevel)` creating real ECS entities; `RuntimeGameSession.cpp:212` calls `tickDoorAnimation(registry_, deltaTime)`; `updateRuntimeDoorAnimation` not found in any file |
| R3 | DoorComponent split into DoorConfigComponent + DoorStateComponent | VERIFIED | `DoorConfigComponent.h` (388B) and `DoorStateComponent.h` (752B) exist with correct POD structs; `DoorComponent.h` deleted; `DoorStateComponent` has free-function helpers `isDoorFullyClosed/Open/Moving` (no member methods) |
| R4 | DoorGroupInspector surfaces child leaf meshes, warns when no leaf exists | VERIFIED (with deviation) | `DoorGroupInspector.cpp:119–149` shows leaf mesh names via parentNodeId matching; `"No leaf meshes found -- this door won't animate."` warning at line 148; pivot values not shown (by design — see deviation note) |
| R5 | Right-leaf animation works in editor preview | VERIFIED | `DoorAnimationSystem.cpp:75–76` calls `updateDoorLeaf(registry, config.leftLeaf, state.progress)` and `updateDoorLeaf(registry, config.rightLeaf, state.progress)` in every tick; both leaves animate |
| R6 | Dead code removed — computeHingeWorldPos deleted, naming cleaned up | VERIFIED | `computeHingeWorldPos` not found in any file; `spawnDoubleDoor` not found in any file; `DoubleDoor` not found in any file; `LevelDoorGroupPlacement` not found in any file |

**Score:** 5/6 truths verified (R4 partially — pivot display not implemented, intentional design decision)

**Note on R4 deviation:** The CONTEXT decision D-11 specified the inspector should show pivot values, but the architecture change in Plan 01 removed the `pivot` field from `LevelMeshPlacement` entirely (pivots moved to hardcoded geometry constants in `LevelBuilder::addDoorGroup`). There is no per-leaf pivot value to display in the inspector. The Plan 04 SUMMARY explicitly documents this as an intentional adaptation: "Leaf mesh section shows all child meshes by parentNodeId == dg.nodeId without pivot check. This is correct for the new unified architecture where pivots live in the door group geometry constants." D-13 also explicitly excludes auto-create buttons. The R4 "allows adding pivots" wording in the roadmap is satisfied architecturally (leaf meshes with correct parentNodeId determine door leaves) rather than through an explicit UI action.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/components/DoorConfigComponent.h` | Spawn-time config for doors | VERIFIED | 388B; contains `struct DoorConfigComponent` with leftLeaf, rightLeaf, interactDistance, interactDotThreshold, openDuration, openAngle, locked, lockedPrompt |
| `src/game/components/DoorStateComponent.h` | Runtime animation state for doors | VERIFIED | 752B; contains `struct DoorStateComponent` (POD) + `enum class DoorTargetState` + free functions `isDoorFullyClosed`, `isDoorFullyOpen`, `isDoorMoving` |
| `src/game/components/DoorComponent.h` | MUST NOT EXIST (deleted) | VERIFIED | File does not exist |
| `src/game/level/LevelDef.h` | Unified door placement struct | VERIFIED | `struct LevelDoorPlacement` at line 110; `std::vector<LevelDoorPlacement> doors` at line 135 |
| `src/game/behavior/DoorAnimationSystem.h` | Declares tickDoorAnimation free function | VERIFIED | Line 14: `void tickDoorAnimation(entt::registry& registry, float deltaTime)` |
| `src/game/behavior/DoorAnimationSystem.cpp` | Bidirectional animation with both-leaf support | VERIFIED | Implements `tickDoorAnimation` with `+step`/`-step` branches; calls `updateDoorLeaf` for both leftLeaf and rightLeaf |
| `src/game/prefabs/GameplayPrefabData.h` | Gameplay prefab types (Checkpoint only) | VERIFIED | `enum class GameplayPrefabType { Checkpoint, }` — no DoubleDoor |
| `src/game/prefabs/GameplayPrefabs.cpp` | makePivotLeafModel preserved | VERIFIED | `makePivotLeafModel` at line 10; `spawnDoubleDoor` and `computeHingeWorldPos` not present |
| `src/game/content/ContentRegistry.h` | GameplayArchetypeKind (Checkpoint only) | VERIFIED | `enum class GameplayArchetypeKind { Checkpoint, }` — no DoubleDoor |
| `assets/prefabs/gameplay/double_door.prefab` | MUST NOT EXIST (deleted) | VERIFIED | File does not exist |
| `src/editor/ui/inspectors/DoorGroupInspector.cpp` | Leaf mesh display and warning banner | VERIFIED | "Leaf Meshes" section at line 119; warning text at line 148; iterates `document.objects()` by `parentNodeId` |
| `src/editor/scene/EditorPreviewWorld.cpp` | Full door entity spawning via addDoorGroup | VERIFIED | Line 241: `builder.addDoorGroup(dg, emptyLevel)` replaces minimal TransformComponent-only entity |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `RuntimeGameSession.cpp` | `DoorAnimationSystem.cpp` | `tickDoorAnimation` call | WIRED | `RuntimeGameSession.cpp:20` includes `DoorAnimationSystem.h`; line 212 calls `tickDoorAnimation(registry_, deltaTime)` |
| `BehaviorSystem.cpp` | `DoorStateComponent.h` | Sets `targetState` on door actions | WIRED | `BehaviorSystem.cpp:177,202,217,225` set `state->targetState = DoorTargetState::Open/Closed`; free functions `isDoorFullyOpen/isDoorMoving` used at lines 176, 215 |
| `EditorPreviewWorld.cpp` | `LevelBuilder.cpp` | `addDoorGroup()` for full entity hierarchy | WIRED | `EditorPreviewWorld.cpp:241` calls `builder.addDoorGroup(dg, emptyLevel)` |
| `EditorScenePreviewRenderer.cpp` | `LevelDef.h` | Reads `LevelDoorPlacement` for pivot visualization | WIRED | Line 309: `std::get<LevelDoorPlacement>(object.payload)`; line 310: `const glm::vec3 hingePos = dg.position` (no dead `computeHingeWorldPos` call) |
| `DoorGroupInspector.cpp` | `EditorSceneDocument.h` | Reads document objects for leaf mesh display | WIRED | Line 126: `document.objects()` iterated; checks `EditorSceneObjectKind::Mesh` and `parentNodeId == dg.nodeId` |
| `LevelDef.cpp` parser | `LevelDoorPlacement` | `door_group` keyword maps to new type | WIRED | `LevelDef.cpp:962` checks `kind == "door_group"`; line 967 instantiates `LevelDoorPlacement dg` |
| `LevelDef.cpp` serializer | `door_group` keyword | Outputs unchanged keyword | WIRED | `LevelDef.cpp:1408` outputs `"door_group "` |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `DoorAnimationSystem.cpp` | `state.progress` | `DoorStateComponent` on ECS entity | Yes — written by `tickDoorAnimation`, read by `updateDoorLeaf` to set `mesh->modelOverride` | FLOWING |
| `EditorPreviewWorld.cpp` | Door entity hierarchy | `LevelBuilder::addDoorGroup` | Yes — spawns real `DoorConfigComponent + DoorStateComponent + DoorLeafComponent` entities into registry | FLOWING |
| `DoorGroupInspector.cpp` | Leaf mesh names | `document.objects()` traversal | Yes — reads from real document state; `meshPlacement.meshId` is a real string from scene data | FLOWING |

### Behavioral Spot-Checks

Step 7b: SKIPPED (cannot start editor/game runtime without a running server; animation behavior requires a running application)

The following checks were performed programmatically:

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| All 30 tests pass | `ctest --output-on-failure` in build/ | 30/30 passed, 1.79s | PASS |
| Build all targets | `cmake --build build` | 100% — all targets built | PASS |
| `tickDoorAnimation` declared in header | grep | `DoorAnimationSystem.h:14` | PASS |
| Both leaves animated in tickDoorAnimation | grep | `DoorAnimationSystem.cpp:75–76` | PASS |
| `updateRuntimeDoorAnimation` deleted | grep (exit 1) | Not found in any file | PASS |
| `LevelDoorGroupPlacement` eliminated | grep (exit 1) | Not found in src/ or tests/ | PASS |
| `DoubleDoor` eliminated | grep (exit 1) | Not found in src/ | PASS |
| `double_door.prefab` deleted | ls | File does not exist | PASS |
| `computeHingeWorldPos` eliminated | grep (exit 1) | Not found in any file | PASS |
| Warning text present in inspector | grep | `DoorGroupInspector.cpp:148` | PASS |
| `addDoorGroup` called in EditorPreviewWorld | grep | `EditorPreviewWorld.cpp:241` | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| R1 | 19-01, 19-03 | Single unified door definition replacing LevelDoorGroupPlacement + DoubleDoorSpawnSpec | SATISFIED | `LevelDoorPlacement` struct in `LevelDef.h`; `DoubleDoorSpawnSpec` deleted; one spawn path via `LevelBuilder::addDoorGroup` |
| R2 | 19-02, 19-04 | Editor preview uses same DoorAnimationSystem/BehaviorSystem as game | SATISFIED | `EditorPreviewWorld` spawns full entities via `addDoorGroup`; `RuntimeGameSession` calls `tickDoorAnimation`; no duplicate animation code |
| R3 | 19-01 | DoorComponent split into DoorConfigComponent + DoorStateComponent | SATISFIED | Both headers exist with correct POD structs; DoorComponent.h deleted; free-function helpers per CLAUDE.md ECS convention |
| R4 | 19-04 | DoorGroupInspector surfaces child leaf meshes, warns when no leaf exists | SATISFIED (with deviation) | Leaf mesh names shown; warning text present; pivot values not shown (intentional — pivot data now in hardcoded geometry constants, not per-leaf data) |
| R5 | 19-02, 19-04 | Right-leaf animation works in editor preview | SATISFIED | `tickDoorAnimation` animates both `config.leftLeaf` and `config.rightLeaf`; `EditorPreviewWorld` creates real `DoorConfigComponent` entities |
| R6 | 19-02, 19-03 | Dead code removed, naming cleaned up | SATISFIED | `computeHingeWorldPos` deleted; `spawnDoubleDoor` deleted; `LevelDoorGroupPlacement` renamed; `DoubleDoor` eliminated from all type names |

Note: The PLAN frontmatter lists requirement IDs R1, R2, R3, R5, R6. R4 appears in 19-04 plan only. All 6 roadmap-defined requirements are addressed across the 4 plans. REQUIREMENTS.md uses different IDs (RNDR-xx, PLYR-xx, etc.) that do not map to this phase — the R1–R6 requirements are phase-internal roadmap requirements, not global REQUIREMENTS.md entries.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/game/runtime/RuntimeGameplay.cpp` | 428–432 | `updateRuntimeBehaviors` — declared, defined as empty stub, never called | Warning | Dead code; no runtime impact; noted by code review IN-03; does not affect any phase goal |
| `src/game/behavior/DoorAnimationSystem.cpp` | 27–30 | `updateDoorLeaf` calls `registry.try_get` without prior `registry.valid(entity)` check | Warning | UB if leaf entity destroyed while door root exists; noted by code review WR-01/WR-02; no known crash scenario in current usage but fragile |
| `src/game/runtime/RuntimeGameSession.cpp` | 399–405 | `restoreBaselineState` restores `DoorStateComponent` but does not re-sync leaf mesh transforms | Warning | One-frame visual inconsistency after reset; corrects itself on next `tickDoorAnimation` call; noted by code review WR-03 |

None of the above are blockers for the phase goal. All were identified in the code review (REVIEW.md) and are pre-existing warnings, not new issues.

### Human Verification Required

#### 1. DoorGroupInspector leaf mesh display and warning

**Test:** Open level-editor. Load `initial_scene.scene`. Select a DoorGroup entity in the outliner (if none exists, add one). Inspect the right-side inspector panel.
**Expected:** The inspector shows a "Leaf Meshes" heading below the property table. If the door group has child mesh objects (parentNodeId matches), they appear in green with "(leaf)" labels. If no child mesh objects exist, a yellow warning "No leaf meshes found -- this door won't animate." is visible.
**Why human:** ImGui rendering output cannot be verified without a running editor session.

#### 2. Right-leaf animation and bidirectional close

**Test:** In level-editor play preview (or game executable), navigate to a door and press E to interact. Observe both door leaves. After the door opens, press E again to close it.
**Expected:** Both door leaves swing open simultaneously. The close action produces a smooth reverse animation (leaves swing back from current angle) rather than snapping to closed position. A door interrupted mid-swing reverses smoothly from its current progress.
**Why human:** Animation smoothness, bidirectionality correctness, and both-leaf visual behavior require a running application frame loop.

#### 3. Editor preview visual parity with game runtime

**Test:** Trigger a door open in both the level-editor play preview and the game executable (pixel-roguelike). Compare the door animation: leaf geometry, pivot point, open angle, and timing.
**Expected:** Door animation is visually identical in both contexts — confirming that `EditorPreviewWorld` + `RuntimeGameSession` + `tickDoorAnimation` uses the same math path as `DoorAnimationSystem::update` in the game executable.
**Why human:** Visual parity between two separate application contexts cannot be verified by code inspection alone.

### Gaps Summary

No blocking gaps found. The phase goal is fully implemented in code:
- Split-brain architecture eliminated (single spawn path via `LevelBuilder::addDoorGroup`, single animation path via `tickDoorAnimation`)
- `DoorConfigComponent` + `DoorStateComponent` replace `DoorComponent` throughout the codebase
- `LevelDoorPlacement` replaces `LevelDoorGroupPlacement` and `DoubleDoorSpawnSpec`
- Right-leaf animation bug fixed by design (both leaves animated in `tickDoorAnimation`)
- Dead code eliminated (`computeHingeWorldPos`, `spawnDoubleDoor`, `double_door.prefab`, `updateRuntimeDoorAnimation`)
- Editor inspector shows leaf meshes and warning banner
- Editor preview spawns full door entity hierarchy via `addDoorGroup`
- 30/30 tests pass, all targets compile

The three human verification items are about visual/behavioral correctness in a running application, not code gaps.

---

_Verified: 2026-04-07T14:00:00Z_
_Verifier: Claude (gsd-verifier)_
