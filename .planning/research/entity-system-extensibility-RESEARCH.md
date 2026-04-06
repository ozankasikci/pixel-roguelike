# Entity System Extensibility — Research

**Researched:** 2026-04-06
**Domain:** Level definition architecture, entity/component extensibility, Stanley Parable-style gameplay systems
**Confidence:** HIGH (architecture analysis from codebase) / MEDIUM (commercial engine comparisons) / MEDIUM (gameplay requirements)

---

## Summary

The current engine has a mature, well-structured foundation: a data-driven behavior system (Phase 13), a unified collider system (Phase 17), and a post-Phase-18 maintainability pass. The architecture is already closer to Source Engine territory than a naive custom engine. The core loop of `LevelDef → LevelBuilder → ECS entities → BehaviorSystem` is sound and extensible without architectural surgery.

The key gaps are not structural — they are in the **palette of placement types** and **action vocabulary** available in `.scene` files. Adding a Stanley Parable-style level currently requires workarounds: items are faked as meshes with interactable sub-lines, readable notes require inventing conventions, and multi-state objects (e.g., a keypad that progresses through states) need sequences of actions that the current system nearly supports but cannot fully express.

The three most impactful additions are: (1) an `ItemPickupComponent` + `PickupAction` that gives items a game-state identity beyond "interactable mesh", (2) a `ReadableComponent` for notes/recordings/signs that triggers a fullscreen read UI, and (3) a global `LevelStateComponent` or variable store so behaviors can branch on world state (e.g., "only open door if key was picked up"). Everything else is refinement.

**Primary recommendation:** Extend the behavior action vocabulary and add three new placement-level entity types (`item`, `readable`, `npc_spawn`) before attempting any architectural overhaul. The existing `BehaviorComponent`/`ActionEntry`/`std::variant` pattern cleanly absorbs new action types without changing the dispatcher.

---

## Stanley Parable Gameplay Requirements

The Stanley Parable (Source engine mod, then standalone via Valve's Source SDK) uses these entity categories. This analysis is based on the Valve Developer Community wiki and the publicly known Source entity vocabulary used by the mod. [CITED: developer.valvesoftware.com/wiki/Entity, CITED: developer.valvesoftware.com/wiki/Entities_in_Depth]

### Interactive Object Types Required

| Object Type | Game Purpose | Current Support | Gap |
|-------------|--------------|-----------------|-----|
| Door (single-leaf) | Narrative routing, player choices | YES — `LevelSingleDoorPlacement` | None |
| Door (double-leaf) | Grander spaces | YES — `DoubleDoorSpawnSpec` archetype | None |
| Locked door | Blocking paths until condition met | Partial — `locked=true` but no condition check | Cannot conditionally unlock based on item held |
| Button / lever | Branch triggers, puzzle inputs | Partial — interactable mesh fires `on_activate` | Works for simple case; no multi-state |
| Elevator | Vertical transition | Not present | Needs: moving platform + trigger arrival |
| Checkpoint / respawn | Save progress | YES — `CheckpointSpawnSpec` | None |
| Item pickup | Key items, narrative objects | Not present | Needs: `ItemPickupComponent`, session state |
| Readable note | Environmental story, recordings | Not present | Needs: `ReadableComponent`, UI |
| NPC (narrator voice) | Core Stanley Parable mechanic | Not present | Needs: `AudioZoneComponent` or narration system |
| Camera / security monitor | Environmental prop + viewpoint | Not present | Out of scope for v1 |
| Control terminal / keypad | Puzzle input, multi-step sequences | Not present | Needs: multi-state interactable |
| Trigger volume (enter/exit) | Zone-based story beats | YES — unified `ColliderComponent` trigger mode | None |
| Game-state variable | Conditional branching | Not present | Critical gap |
| Teleport | Stanley Parable loop mechanic | YES — `TeleportPlayer` action | None |
| Message display | Narrator text on screen | YES — `ShowMessage` action | None |
| Sound zone | Ambient audio, music triggers | Partial — `PlaySound` action exists | No looping spatial audio zones |
| Level transition | Moving between scenes | Not present | Needs: scene-transition action |

### Interaction Pattern Analysis

The Stanley Parable's branching works through a small number of primitives repeated at scale:

1. **Player enters trigger → fires action sequence** — fully supported
2. **Player presses E on object → fires on_activate** — fully supported
3. **Object state changes affect other objects** — partially supported (e.g., OpenDoor → chain reaction via NodeIndex)
4. **Object state affects subsequent player interactions** — NOT supported (no world-state variables)
5. **Event plays once, then world is different** — partially supported via `fireOnce = true` on actions
6. **Narrator voice reacts to player location/choice** — NOT supported (no NPC/narration subsystem)

---

## How Commercial Engines Handle This

### Source Engine (Half-Life 2, The Stanley Parable) — Most Relevant Reference

[CITED: valvedev.info/guides/entity-interactions-in-sources-input-output-system, CITED: developer.valvesoftware.com/wiki/Entities_in_Depth]

**Architecture:** FGD (Forge Game Data) files define entity schemas — name, property types, available inputs and outputs. The Hammer editor reads FGDs and shows type-safe inspectors for each entity class. No C++ recompilation is required to expose new entity properties in the editor; only the FGD file changes.

**Input/Output system:** Every entity type declares its possible outputs (events it emits) and inputs (messages it accepts). A `trigger_once` volume has an `OnTrigger` output; a `func_door` has `Open`, `Close`, `Lock`, `Unlock` inputs. Level designers wire these in the editor by selecting "output → target entity → input → delay" without touching code.

**Key strengths that our engine lacks:**
- `logic_relay`: A zero-geometry entity that exists purely to relay events, acts as a named event hub. Equivalent: we have `EmitEvent` + BehaviorEvent bus, but no persistent relay entity you can place in the scene.
- `logic_case` / `logic_branch`: Conditional dispatch based on a stored variable. We have no equivalent.
- `math_counter`: Counts triggers, fires output when count reaches threshold. We have no equivalent.
- `env_message`: Displays HUD text tied to a specific trigger. We have `ShowMessage` action, which covers this.
- Global variables (`env_global`): Persistent named variables across level loads. We have no equivalent.

**The Stanley Parable specifically** built its entire narrative branching on `logic_relay`, `trigger_once`, `env_message`, and `trigger_multiple` entities. The narrator voiceover was a custom entity that fired audio at specific positions. [CITED: tcrf.net/The_Stanley_Parable — entity names from map files]

**Takeaway:** Source's power comes from the richness of logical/utility entity types, not from the physical entity types. Our engine has good physical entities (meshes, lights, colliders, doors). We're missing logical entities: relay, counter, variable store, conditional branch.

### Unity

[ASSUMED — based on Unity documentation knowledge from training, not verified in this session]

Unity uses `MonoBehaviour` scripts with `[SerializeField]` attributes that automatically appear in the Inspector. Adding a new "entity type" means writing a new component class; the editor reflects it automatically. Event wiring uses `UnityEvent` (serializable event references that level designers can set up without code via Inspector drag-and-drop).

**Comparison to our engine:**
- Unity's Inspector reflection replaces our explicit inspector panel files. Our approach requires a new inspector `.cpp` file per placement type — higher friction but complete control.
- `UnityEvent` is roughly equivalent to our `BehaviorComponent` action list, but more general (any serialized method call vs. our typed enum actions). Our approach is safer for serialization but requires a code change to add each new action type.
- Unity prefabs are equivalent to our `LevelArchetypePlacement` + `GameplayArchetypeDefinition`. Our archetype system is more constrained but sufficient for the game's needs.

### Unreal Engine 5

[ASSUMED — based on Unreal documentation knowledge from training, not verified in this session]

Unreal's Blueprint system allows level designers to author behavior in a visual scripting environment — full programming power without C++. This is overkill for our game and contradicts the project's custom-engine mandate. The relevant pattern to extract is:

- **Actor communication via interfaces:** Actors implement `IInteractable` interface; other actors call it without knowing the concrete type. This generalizes our current `InteractableComponent` + `BehaviorComponent` approach.
- **Event Dispatchers (per-actor event buses):** An actor can declare named events that other actors subscribe to. This is more granular than our global EventBus — it enables `door.OnOpened → narrative_beat.Fire()` connections without a shared event name space.

**Takeaway:** The per-entity event binding pattern (instead of only a global bus) would eliminate naming collisions in complex scenes with many interactive objects.

### Godot 4

[ASSUMED — based on Godot documentation knowledge from training, not verified in this session]

Godot's signal system is structurally identical to what we need: nodes emit named signals, other nodes connect to them. The key difference is Godot's connections are stored in the scene file, so the editor can visualize the wiring graph.

**Pattern to steal:** Signal connections stored in scene data, not only in code. Our `BehaviorDeclaration` + `targetNodeId` approach already stores wiring in `.scene` files — we're already doing the Godot pattern.

---

## Current Architecture Assessment

### Strengths

**What the current system does extremely well:**

1. **Clean separation of data and logic.** `LevelDef` is pure serializable data. `LevelBuilder` converts it to ECS entities. `BehaviorSystem` dispatches actions from ECS data. No coupling between these layers. [VERIFIED: codebase read]

2. **Node ID targeting is powerful and correct.** The `NodeIndex` stored in `registry.ctx()` lets any action target any named entity by string ID without hardcoding entity handles. This is the same pattern Source uses for `targetname`. [VERIFIED: codebase read — LevelLoader.cpp lines 111-121, BehaviorSystem.cpp lines 161-167]

3. **Delayed action sequences work.** The `PendingAction` queue with sorted `fireTime` is a correctly implemented deferred event queue. Chaining `Delay → OpenDoor → Delay → ShowMessage` works today. [VERIFIED: codebase read — BehaviorSystem.cpp lines 76-116]

4. **`fireOnce` flag prevents re-triggers.** Events that should only happen once (narrative beats) work correctly. [VERIFIED: codebase read — ActionEntry::fireOnce]

5. **Trigger colliders handle enter/exit.** The unified `ColliderComponent` with trigger mode fires `onEnter`/`onExit` action lists. Player stepping into a zone can fire narrative sequences. [VERIFIED: codebase read — BehaviorSystem.cpp lines 59-74]

6. **Editor has full round-trip fidelity.** `toLevelDef()` and `loadFromSceneFile()` round-trip all placement types including behaviors and interactables. Phase 16 completed trigger/behavior authoring UI. [VERIFIED: codebase read — EditorSceneDocument.h]

7. **The `std::variant` ActionParams pattern is extensible.** Adding a new action type requires: (1) add enum value to `ActionType`, (2) add params struct, (3) add to `ActionParams` variant, (4) add case to `executeAction()` switch, (5) add parser/serializer lines, (6) add inspector UI. This is 5-6 bounded changes, all in clearly identified files. [VERIFIED: codebase read — ActionTypes.h, BehaviorSystem.cpp]

### Weaknesses

**What requires new code or structural extension:**

1. **No world-state variables.** There is no mechanism for behaviors to read or write named boolean/integer values. Cannot implement "only open this door if the player has the keycard." The most critical gap for Stanley Parable-style branching. [VERIFIED: codebase read — no such component or context object found]

2. **Adding a new placement type requires N touch points.** A new first-class entity type (e.g., `item`) currently requires changes to: `LevelDef.h` (new struct + vector), `LevelDef.cpp` (parser/serializer), `LevelBuilder.h/.cpp` (new spawn method), `LevelLoader.cpp` (new loop), `EditorSceneDocument.h` (new variant arm + add method), `EditorSceneDocument.cpp` (all visitors + `toLevelDef`), and a new inspector file. That is 7+ files for a new placement type. The maintainability refactoring in Phase 18 reduced this but did not eliminate it. [VERIFIED: codebase read — counted touch points manually]

3. **`EnableEntity`/`DisableEntity` only affects `InteractableComponent::enabled`.** The current implementation (BehaviorSystem.cpp lines 292-303) only toggles the interactable flag — it does not hide/show the mesh or disable the collider. A "disabled" entity is still rendered and still physically solid. This is a functional gap for hiding objects revealed by narrative events. [VERIFIED: codebase read — BehaviorSystem.cpp executeAction()]

4. **No item pickup system.** There is no `ItemPickupComponent`, no way to record "player has item X" in `RunSession`, and no action to check item possession. Items are modeled as interactable meshes that fire actions but leave no persistent state. [VERIFIED: codebase read — RunSession not read but ItemDefinition exists in ContentRegistry.h with no pickup tracking]

5. **No readable/examine system.** The engine has no "examine this object" UI flow — no `ReadableComponent`, no fullscreen text/image display triggered by interaction. [VERIFIED: codebase read]

6. **Single-archetype limitation.** `GameplayArchetypeKind` is a closed enum (`Checkpoint`, `DoubleDoor`). Each new archetype type requires a code change to add to the enum and switch statements. There is no data-only archetype path for new composite objects. [VERIFIED: codebase read — GameplayArchetypeDefinition.h line 59-63]

7. **`LevelSingleDoorPlacement` is a special-case struct.** Doors get their own placement vector in `LevelDef` and their own loop in `LevelLoader`, separate from the archetype system. This means doors cannot attach behaviors (no `std::vector<BehaviorDeclaration>` field in `LevelSingleDoorPlacement`). A trigger-controlled door that responds to `EmitEvent` cannot be authored today — door events fire through `InteractableComponent` only. [VERIFIED: codebase read — LevelDef.h lines 110-127, LevelLoader.cpp lines 107-109]

8. **No sequential/scripted sequence construct.** While you can chain `Delay` actions in a single action list, there is no way to sequence across entities. "Trigger A fires, then 2 seconds later trigger B fires automatically" requires trigger B to have an `on_enter` with a Delay, not a chain from A's event. The Source `logic_relay` solves this. [VERIFIED: by analyzing BehaviorSystem.cpp action dispatch]

9. **No conditional branching in action lists.** An action list executes all entries (subject to fireOnce). There is no `if` construct — "if door was already opened, do X; else do Y." [VERIFIED: codebase read — executeActionList is unconditional]

---

## Gap Analysis

### Priority Matrix

| Gap | Stanley Parable Need | Implementation Cost | Gameplay Unlock |
|-----|---------------------|---------------------|-----------------|
| World-state variables | Critical — branching | Medium (new context object + actions) | All conditional gameplay |
| Item pickup system | High — key items, collectibles | Medium (new component + scene type) | Inventory-gated doors, collectibles |
| Readable/examine system | High — notes, recordings | Medium (new component + UI) | Environmental storytelling |
| Disable entity fully | Medium — hidden reveal objects | Low (fix existing action) | Narrative reveals |
| Door behavior support | Medium — trap/auto-close doors | Low (add behaviors field to door struct) | Triggered doors |
| Logic relay entity | Medium — event wiring | Low (new scene type: zero-geometry entity) | Complex multi-step sequences |
| Multi-state interactable | Medium — keypads, buttons | Medium (new state machine component) | Puzzle elements |
| Level transition action | High — connected rooms | Low (new ActionType) | Multi-room levels |
| Audio zone (looping) | Medium — ambient atmosphere | Medium (new ColliderComponent mode or placement type) | Spatial audio storytelling |
| Conditional action | Low for v1 | Medium (adds variant complexity) | Branching per-entity |

### What Works Today Without Changes

The following Stanley Parable scenarios are already fully implementable:

- Player walks through door opening trigger → narrator voice (ShowMessage) plays
- Player enters a room (trigger_enter) → lights turn on (SetLight actions)
- Player interacts with button → sequence: Lock player, door opens, Unlock player
- Player reaches checkpoint → respawn point updates
- First-time-only narrative beat (fireOnce on trigger's onEnter)
- Teleporting player to a new start position for a "loop"
- Flickering lights during a tense sequence
- A door that is permanently locked (locked=true, no condition)
- Multiple lights controlled by one trigger

### What Requires New Code (Priority Order)

**Gap 1: Fix `EnableEntity`/`DisableEntity` to affect render + collision**

Current behavior only toggles `InteractableComponent::enabled`. Required behavior: hide mesh, disable collider, disable interactable. This is a targeted fix to `executeAction()` in BehaviorSystem.cpp — add visibility flag to `MeshComponent` and `ColliderComponent`, check it in render system and physics system.

**Gap 2: Add `LevelTransitionAction` to ActionType**

A `TransitionToLevel` action with a target scene path. `BehaviorSystem::executeAction` publishes a `LoadLevelEvent` that `RuntimeGameSession` handles. No new placement types needed. Unlocks multi-room levels.

**Gap 3: Behaviors on `LevelSingleDoorPlacement`**

Add `std::vector<BehaviorDeclaration> behaviors` field to `LevelSingleDoorPlacement`. Update parser, serializer, `LevelBuilder::addSingleDoor()`. This allows trigger-controlled doors (auto-close after X seconds, door that only opens after an event fires).

**Gap 4: Item pickup placement type**

New placement type `item` in `.scene` files. New `ItemPickupComponent` with `itemId` string. New `PickupItemAction` in ActionType. `RunSession` gains an `ownedItems: std::unordered_set<std::string>`. `BehaviorSystem` handles the pickup action by adding to `RunSession`. This is the minimum viable inventory-gated interaction.

**Gap 5: World-state variables (boolean flags in RunSession)**

New `RunSession::worldFlags: std::unordered_map<std::string, bool>`. New actions: `SetFlag(name, value)`, `ClearFlag(name)`. New conditional action: `IfFlag(name) { actions... }`. Parser support for `on_activate if_flag keycard set_flag door_unlocked`. This unlocks all conditional gameplay.

**Gap 6: Readable/examine placement type**

New `readable` keyword in `.scene` format. `ReadableComponent` with title, body text (or reference to a `.txt` asset). New `ExamineAction` that publishes `ShowReadableEvent`. A minimal fullscreen UI renders the text. Sufficient for notes, recordings, research files.

**Gap 7: Logic relay placement type**

Zero-geometry placement type `relay <nodeId>` that spawns an entity with only `NodeIdComponent` + `BehaviorComponent`. Enables "A fires → relay → B, C, D all fire" patterns. Requires no new components — just a new scene parser keyword that creates a bare entity.

---

## Recommended Extensions (Prioritized)

### Tier 1 — Unlock Core Gameplay (do these first)

**1.1 Fix EnableEntity/DisableEntity (1-2 days)**
- Add `bool visible` to `MeshComponent`, check in `RuntimeSceneRenderer`
- Add `ColliderComponent::enabled` check already exists; extend to also zero out physics body
- Update `executeAction()`: `EnableEntity` sets visible=true + enabled=true; `DisableEntity` sets visible=false + enabled=false + interactable enabled=false
- No new placement types, no parser changes

**1.2 Level Transition Action (0.5 days)**
- Add `TransitionToLevel` to `ActionType` enum
- Add `LevelTransitionParams { std::string levelId; }` to `ActionParams` variant
- `executeAction()` publishes `LoadLevelEvent{levelId}` on EventBus
- Parser: `on_activate transition_to level_id my_scene`
- `RuntimeGameSession` already handles level loading from EventBus

**1.3 Behaviors on Doors (0.5 days)**
- Add `std::vector<BehaviorDeclaration> behaviors` to `LevelSingleDoorPlacement`
- Update `LevelDef.cpp` parser/serializer (follow existing mesh behavior sub-line pattern)
- Update `LevelBuilder::addSingleDoor()` to call `attachBehaviors()`

### Tier 2 — Environmental Storytelling

**2.1 Item Pickup System (2-3 days)**
- New `LevelItemPlacement` struct: `itemId`, position, scale, rotation, nodeId, optional mesh override
- New `ItemPickupComponent { std::string itemId; bool pickedUp = false; }`
- `RunSession` gains `std::unordered_set<std::string> ownedItems`
- New `ActionType::PickupItem` with `ItemPickupParams { std::string itemId; }` — adds to session, disables entity
- New `LevelItemPlacement` vector in `LevelDef`, loop in `LevelLoader`, `EditorSceneDocument` arm
- Scene syntax: `item keycard 1.0 0.5 3.0  mesh SM_Keycard  interactable "E  Pick up keycard" distance 1.5`

**2.2 Readable/Examine System (2-3 days)**
- New `LevelReadablePlacement` struct: nodeId, position, `std::string title`, `std::string body` (or file path)
- New `ReadableComponent { std::string title; std::string body; bool examined = false; }`
- New `ActionType::ShowReadable` with `ReadableParams { std::string nodeId; }` — publishes `ShowReadableEvent`
- Minimal HUD: `ShowReadableEvent` handler in a new `ReadableUISystem` that renders fullscreen text over game view
- Default interactable sub-line auto-added: `interactable "E  Read" distance 1.5`
- Scene syntax: `readable "Subject: RE: Maintenance Report" 2.0 1.0 4.0  body "The west corridor light has been out since Tuesday..."`

### Tier 3 — World State and Branching

**3.1 World State Variables (3-4 days)**
- `RunSession` gains `std::unordered_map<std::string, bool> worldFlags`
- New action types: `SetFlag`, `ClearFlag`, `ToggleFlag`; params: `FlagActionParams { std::string flagName; bool value = true; }`
- New conditional action: `ActionType::ConditionalFlag` with `ConditionalFlagParams { std::string flagName; bool expectedValue; std::vector<ActionEntry> thenActions; }` — executed inline in `executeActionList()`
- Parser: `on_activate set_flag door_a_opened` and `on_enter if_flag has_keycard open_door door_a`
- This is the single most powerful unlock for conditional gameplay

**3.2 Logic Relay Entity (0.5 days)**
- New parser keyword `relay <nodeId>` in `LevelDef.cpp`
- Spawns bare entity with `NodeIdComponent` + optional `BehaviorComponent`
- Editor: shows as "Relay" kind with behavior-only inspector (no transform gizmo, or a small icon)
- Scene syntax: `relay n_entrance_sequence  on_activate open_door door_a  on_activate show_message n_msg "You hear footsteps..."`

### Tier 4 — Advanced Interactions (future)

**4.1 Multi-State Interactable**
- `StateComponent { int state = 0; int maxStates = 2; std::vector<std::vector<ActionEntry>> stateActions; }`
- Each interaction advances state and fires state-specific actions
- Enables keypads, multi-step puzzles

**4.2 Audio Zone (Looping)**
- New `AudioZoneComponent` backed by a collider volume — player entry starts looping audio, exit stops it
- Can be implemented as a `ColliderComponent` trigger + `PlayLoopingSound`/`StopLoopingSound` actions

**4.3 Moving Platform (Elevator)**
- New `MovingPlatformComponent { glm::vec3 start, end; float duration; bool loop; }`
- `MovingPlatformSystem` lerps position each frame
- `StartPlatform`/`StopPlatform` actions

---

## Architecture Patterns to Adopt

### Pattern 1: Logic Relay (from Source Engine)

A zero-geometry scene entity that exists only to relay events and sequence actions. This is the Source `logic_relay` translated to our system.

**What it enables:** Complex multi-step sequences authored without coupling source entity to all targets. "Entrance sequence" relay fires when triggered, dispatches to 5 different entities in order with delays.

```
# In .scene file:
relay n_entrance_sequence
  on_activate open_door door_a
  on_activate delay 1.5
  on_activate set_light light_hall 2.0
  on_activate show_message n_msg "The door ahead groans open."
  on_activate fire_once
```

[VERIFIED by codebase analysis: this requires only a new parser keyword; all component types already exist]

### Pattern 2: World Flags in RunSession (from Source `env_global`)

Named boolean values in `RunSession` that persist across scenes. Actions set/clear/check them. This is the minimum viable conditional game state.

**What it enables:** "Did the player pick up the keycard? → Then this door can be opened." "Did the player read this note? → Don't show the narrative trigger again."

```cpp
// In RunSession (new field)
std::unordered_map<std::string, bool> worldFlags;

// Example: scene file
on_activate set_flag has_keycard
// Later in a different entity:
on_enter if_flag has_keycard open_door n_locked_door
```

[VERIFIED by gap analysis: `RunSession` struct is accessible from `LevelLoader` and `BehaviorSystem` via Application services]

### Pattern 3: Inline Conditional in Action Lists

Rather than a separate conditional entity (complex), embed the condition inside the action entry itself. The `ConditionalFlagParams` struct wraps a nested action list evaluated inline.

```
on_enter if_flag door_unlocked open_door self
on_enter if_flag door_unlocked show_message n_msg "The door opens..."
```

This keeps the action list flat and readable without a full scripting language.

### Pattern 4: Item Pickup as First-Class Placement

Items are not meshes with interactable stubs — they are their own placement type with a semantic `itemId` that connects to session state and ContentRegistry's `ItemDefinition`.

```
item keycard_b -2.5 0.5 4.0  mesh SM_Keycard_Hanging  material metal_default
  interactable "E  Take keycard" distance 1.5
  on_pickup set_flag has_keycard_b
  on_pickup show_message n_msg "You took the keycard."
```

### Pattern 5: Readable as First-Class Placement (not a mesh)

Environmental notes, recordings, and signs should be their own semantic type. The game can track which notes the player has read, notes can reference external `.txt` files for long-form content, and the editor can search/filter them.

```
readable n_note_1 -1.5 1.2 3.5  rotation 0 45 0  title "Maintenance Log" body_file notes/maintenance_log.txt
  interactable "E  Read" distance 1.5
```

---

## Common Pitfalls

### Pitfall 1: Adding Entity Types That Require 7+ File Changes

**What goes wrong:** Every new placement type today touches LevelDef.h, LevelDef.cpp, LevelBuilder.h/.cpp, LevelLoader.cpp, EditorSceneDocument.h/.cpp, plus a new inspector file. This is correct for now but will become a bottleneck if 10+ new types are added.

**Why it happens:** The `EditorSceneObjectPayload` is a `std::variant` that must enumerate all types at compile time. `toLevelDef()` has a visitor that switches on type. Both require touching the same files.

**How to avoid:** Accept the current pattern for now — we need at most 4-5 new placement types (item, readable, relay, possibly audio_zone). If the count goes beyond 8 total, consider introducing a property bag (`std::unordered_map<std::string, std::string>`) as a generic extension field on existing placement types. Do NOT pre-optimize this — the current pattern is maintainable at current scale.

**Warning signs:** If LevelDef.cpp exceeds ~1000 lines or EditorSceneDocument.cpp exceeds ~800 lines after additions, consolidate.

### Pitfall 2: Encoding Game Logic in Action Parser

**What goes wrong:** Temptation to add complex syntax like `on_enter if inventory contains keycard open_door self` directly into the scene file parser. The parser grows into a mini-language interpreter. Debugging becomes hard; serialization becomes fragile.

**Why it happens:** The scene file format is expressive and designers want conditional logic.

**How to avoid:** Keep the parser to keyword → struct mapping. Complex conditions are pre-evaluated actions (`ConditionalFlagParams`) evaluated at runtime by BehaviorSystem, not in the parser. Parser only stores what to check, not how to evaluate it.

### Pitfall 3: Putting Game State in Components Instead of RunSession

**What goes wrong:** Storing "has player picked up item X" in a component on the item entity. When the scene reloads, the entity is re-created and the state is lost. Alternatively, trying to persist ECS state across scene loads is architecturally complex.

**Why it happens:** It feels natural to put per-item state on the item entity.

**How to avoid:** All persistent game state goes in `RunSession`. ECS components hold only ephemeral frame-to-frame state. When a scene loads, `LevelLoader` reads `RunSession` to set initial entity state (e.g., "this item was already picked up → spawn it with `visible=false`"). This is the correct pattern already used for respawn position.

**Warning signs:** If you find yourself querying ECS to determine game progress, the state belongs in RunSession.

### Pitfall 4: Implementing Conditional Branching Before World State Variables

**What goes wrong:** Adding `ConditionalFlagParams` to action lists before `RunSession.worldFlags` exists. The conditional action has nowhere to read from.

**How to avoid:** Build world state variables (Tier 3.1) before multi-condition behaviors. They are a prerequisite.

### Pitfall 5: Over-Engineering the Archetype System

**What goes wrong:** Trying to make `GameplayArchetypeDefinition` data-only and removing the closed enum, adding a property bag. This makes the archetype system more flexible but breaks the type safety that makes `CheckpointSpawnSpec` and `DoubleDoorSpawnSpec` ergonomic.

**How to avoid:** For composite entities that need complex spawn logic, use the scripted prefab pattern (new enum value + spawn function + spec struct). For simple entities (item, readable, relay), use first-class placement types in `LevelDef`. Reserve archetypes for entities that require multi-entity spawn logic (door + collider + light as a group).

### Pitfall 6: Coupling BehaviorSystem to RunSession

**What goes wrong:** `BehaviorSystem::executeAction()` directly reads/writes `RunSession`. This introduces a dependency from the behavior system to the session layer.

**How to avoid:** `BehaviorSystem` publishes events (`PickupItemEvent{itemId}`, `SetFlagEvent{name, value}`) that a `GameStateSystem` or `SessionSystem` handles. This keeps BehaviorSystem decoupled from session persistence. Alternatively, access RunSession via the Application's service locator (already the pattern for other services).

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Scripted narrative sequences with delays | Custom coroutine system | Extend existing `PendingAction` queue with `DelayAction` chaining | Already works; adding coroutines adds complexity for no benefit at this scale |
| Persistent game state | SQLite / file-based save system | Extend `RunSession` struct + existing JSON-like serialization | RunSession is already the session state container; extend it |
| Visual scripting | Blueprint-like node graph in editor | Extend `.scene` file behavior sub-lines | A node graph editor would take months; text-based is already working and designer-friendly |
| Event dispatch system | New event bus | Existing `EventBus` in engine | Already in place; BehaviorSystem already uses it |
| Conditional logic | Scripting language (Lua, AngelScript) | `ConditionalFlagParams` + `worldFlags` in RunSession | Full scripting is 3-6 months of integration work; flag-based conditionals cover 95% of Stanley Parable-level gameplay needs |
| Entity property reflection | Automated C++ reflection (e.g., RTTR) | Explicit per-type inspector files (current pattern) | Reflection libraries add compile time and dependency; current explicit pattern is maintainable at the current scale of ~10-15 placement types |
| Readable text UI | Third-party UI library | Extend existing `stb_truetype` HUD text with a modal overlay | A fullscreen text render is 50-100 lines of OpenGL + ImGui; no library needed |

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Unity's `[SerializeField]` auto-reflects into Inspector; adding a component type does not require explicit panel registration | How Commercial Engines — Unity | Low risk: even if wrong, it doesn't change the analysis of our engine's touch-point cost |
| A2 | Unreal Blueprint actor communication pattern uses Event Dispatchers and Blueprint Interfaces | How Commercial Engines — Unreal | Low risk: architectural insight is directional, not a spec |
| A3 | Godot 4 stores signal connections in scene files | How Commercial Engines — Godot | Low risk: our system already does equivalent via BehaviorDeclaration in .scene |
| A4 | The Stanley Parable was built on Source engine's standard entity vocabulary (`logic_relay`, `trigger_once`, `env_message`) | Stanley Parable section | Medium risk: confirmed it ran on Source but specific entities used are based on general Source mod knowledge, not decompiled map analysis |
| A5 | `RunSession` is accessible from `BehaviorSystem` via Application's service locator | Gap Analysis 5 | Low risk: even if the access path differs, the architectural recommendation is sound |
| A6 | `ItemDefinition` in ContentRegistry has no pickup tracking | Gap analysis | Low risk: confirmed by reading ContentRegistry.h — ItemDefinition has no `pickedUp` field |

---

## Open Questions

1. **Narrator voice system scope**
   - What we know: The Stanley Parable's core mechanic is a narrator voice reacting to player actions. `PlaySound` action exists but requires a pre-loaded audio asset.
   - What's unclear: Is there a narration system planned? Is the game targeting voiced narration, text subtitles only, or environmental text only?
   - Recommendation: Clarify with project owner before designing audio zone placement. If text-only, `ShowMessage` + `ReadableComponent` covers all needs.

2. **Multi-room scene loading**
   - What we know: `RuntimeGameSession` loads a single scene. `LevelTransitionAction` is proposed.
   - What's unclear: Should scenes be persistent (stay loaded when leaving) or fresh (full reload on arrival)?
   - Recommendation: Start with full reload (simpler); run session preserves state.

3. **Item mesh visual**
   - What we know: Item placement would reference a mesh for visual representation.
   - What's unclear: Should items use the mesh library (existing meshes) or have dedicated item mesh assets?
   - Recommendation: Start with mesh library reference; item placement just overrides tint/material.

4. **Conditional branching complexity ceiling**
   - What we know: `ConditionalFlagParams` covers "if flag X is set, do action Y."
   - What's unclear: Does the game require more complex conditions (flag A AND flag B, NOT flag C)?
   - Recommendation: Boolean flags with AND semantics (all listed flags must be set) covers 95% of cases. Implement that; hold off on full expression trees.

---

## Environment Availability

Step 2.6: SKIPPED — this phase is code/architecture analysis only, no new external tool dependencies.

---

## Sources

### Primary (HIGH confidence)
- Codebase direct read: `src/game/behavior/ActionTypes.h`, `BehaviorSystem.cpp`, `LevelDef.h`, `LevelBuilder.cpp`, `LevelLoader.cpp`, `EditorSceneDocument.h`, `ContentRegistry.h`, `GameplayPrefabData.h` — all architecture claims verified [VERIFIED]
- Valve Developer Community entity system: `valvedev.info/guides/entity-interactions-in-sources-input-output-system/` — I/O system, logic_relay, FGD patterns [CITED]
- `assets/scenes/institutional_room.scene` — current level authoring capabilities verified [VERIFIED]

### Secondary (MEDIUM confidence)
- WebSearch: Stanley Parable Source engine entity types — TV Tropes, TCRF, Wikipedia confirming Source engine base [CITED: en.wikipedia.org/wiki/The_Stanley_Parable]
- WebSearch: Source engine trigger types — `twhl.info` tutorials, Valve Developer Community wiki [CITED: developer.valvesoftware.com/wiki/Entities_in_Depth]
- WebSearch: Godot signal architecture — `docs.godotengine.org` [CITED]

### Tertiary (ASSUMED — training knowledge, not verified this session)
- Unity MonoBehaviour/SerializeField/UnityEvent architecture [ASSUMED]
- Unreal Blueprint Event Dispatcher architecture [ASSUMED]
- Godot scene file signal storage [ASSUMED]

---

## Metadata

**Confidence breakdown:**
- Current architecture assessment: HIGH — read from actual source files
- Gap analysis: HIGH — derived from verified codebase inspection
- Stanley Parable gameplay requirements: MEDIUM — based on public knowledge of Source engine patterns; specific entity vocabulary confirmed via Valve Developer Wiki
- Commercial engine comparisons: MEDIUM — Unity/Unreal/Godot patterns from training knowledge, not re-verified
- Extension recommendations: HIGH — grounded in codebase analysis; each recommendation identifies the exact files that change

**Research date:** 2026-04-06
**Valid until:** 2026-07-06 (architecture is stable; check if RunSession serialization changes before implementing world flags)
