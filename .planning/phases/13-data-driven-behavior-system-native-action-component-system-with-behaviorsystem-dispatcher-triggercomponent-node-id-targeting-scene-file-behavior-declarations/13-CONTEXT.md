# Phase 13: Data-driven Behavior System - Context

**Gathered:** 2026-04-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace per-behavior System classes (DoorSystem, CheckpointSystem) with a single BehaviorSystem that dispatches actions from data-driven BehaviorComponent action lists. Add TriggerComponent for spatial trigger volumes with on_enter/on_exit events. Extend the .scene file format with indented sub-lines for behavior declarations. Build a NodeIndex for entity name resolution at load time. Migrate existing scenes to use the new behavior system.

</domain>

<decisions>
## Implementation Decisions

### Action Type Scope
- **D-01:** Initial action types: OpenDoor, CloseDoor, ToggleDoor, PlaySound, SetLight, FlickerLight, ShowMessage, Delay (core interactions)
- **D-02:** Include entity control actions: EnableEntity, DisableEntity, EmitEvent (allows chaining — opening door A disables door B)
- **D-03:** Include player control actions: LockPlayer, UnlockPlayer, TeleportPlayer (PlayerInteractionLockComponent already exists)
- **D-04:** Defer ChangeScene action to a future phase (scene transitions not needed yet)

### Action Parameters
- **D-05:** Use typed `std::variant` for action-specific parameter blocks (e.g., `DoorActionParams`, `SoundActionParams`, `LightActionParams`) — compile-time type safety preferred over generic float/string slots

### Scene File Syntax
- **D-06:** Behavior declarations use indented sub-lines (2-space indent) under their parent entity line
- **D-07:** `interactable` also moves to indented sub-line syntax for consistency — existing .scene files must be migrated
- **D-08:** Parser refactoring required: detect leading whitespace and attach sub-lines to the previous entity

### Migration Strategy
- **D-09:** Keep animation systems, replace activation: BehaviorSystem handles activation dispatch (press E → fire action list). DoorSystem becomes DoorAnimationSystem (only animates door.opening state each frame). CheckpointSystem similar split.
- **D-10:** Migrate ALL existing .scene files to use behavior declarations for doors — proves the system end-to-end with real content
- **D-11:** Delete the activation-checking code from DoorSystem/CheckpointSystem free functions — only animation update remains

### Trigger Volumes
- **D-12:** Include trigger volumes in this phase — TriggerComponent with box and sphere shapes, TriggerSystem checks player overlap each frame
- **D-13:** Manual AABB/sphere overlap checks against player position — no Jolt Physics dependency for triggers
- **D-14:** Triggers are invisible in-game; in the level editor, show semi-transparent wireframe box/sphere for positioning
- **D-15:** Both repeating and fire_once trigger modes — TriggerComponent has a fireOnce flag, default is repeating

### Node Index
- **D-16:** NodeIndex (std::unordered_map<std::string, entt::entity>) built at level load time by LevelBuilder — resolves "self" and named node IDs for action targeting

### Claude's Discretion
- Implementation details of the delayed action queue (priority queue vs sorted vector)
- BehaviorComponent event list naming (onActivate, onEnter, onExit, onTimer)
- Whether to use entt::meta reflection or keep dispatch as a switch statement
- Editor inspector display of behaviors (read-only is acceptable for this phase)
- Exact wireframe rendering approach for trigger volume debug visualization in editor

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Existing Interaction System
- `src/game/ui/InteractionFocusState.h` — Current activation flow: InteractionSystem sets focused + activationRequested, per-system checks and consumes
- `src/game/runtime/RuntimeGameplay.h` — Free functions for interaction, doors, checkpoints — the code being refactored
- `src/game/runtime/RuntimeGameplay.cpp` — Full implementation of door activation, animation, checkpoint activation logic
- `src/game/systems/InteractionSystem.cpp` — Thin wrapper calling updateRuntimeInteraction()
- `src/game/systems/DoorSystem.cpp` — Thin wrapper calling updateRuntimeDoors()
- `src/game/systems/CheckpointSystem.h` — Thin wrapper calling updateRuntimeCheckpoints()

### Components Being Extended/Replaced
- `src/game/components/InteractableComponent.h` — Current interactable POD struct (promptText, distance, dotThreshold, enabled, busy)
- `src/game/components/DoorComponent.h` — Current door state (leftLeaf, rightLeaf, openDuration, progress, opening, opened)
- `src/game/components/CheckpointComponent.h` — Current checkpoint state (respawnPosition, active, lightEntity)

### Scene File System
- `src/game/level/LevelDef.h` — All placement structs and LevelDef; parser will need new BehaviorDeclaration fields
- `src/game/level/LevelDef.cpp` — Line-based .scene parser; needs indented sub-line support
- `src/game/level/LevelLoader.h` — LevelLoadRequest and LevelLoadArgs; LevelBuilder invocation
- `src/game/level/LevelLoader.cpp` — Where placement structs are iterated and entities spawned

### Engine Infrastructure
- `src/engine/core/EventBus.h` — RAII SubscriptionToken, type-safe pub/sub for EmitEvent action
- `src/game/scenes/GenericFileScene.cpp` — ScriptedGeometry registry pattern; institutional_room scripted geometry that builds doors

### Scene Files to Migrate
- `assets/scenes/initial_scene.scene` — Primary test scene
- `assets/scenes/country_house.scene` — Country house scene with doors

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `InteractionFocusState` — actor/focused/activationRequested/activationConsumed pattern; BehaviorSystem replaces per-system consumption
- `EventBus` with `SubscriptionToken` — ready for EmitEvent action type
- `PlayerInteractionLockComponent` — already exists for LockPlayer/UnlockPlayer actions
- `DoorLeafComponent` + animation math (getDoorLeafYaw, makeDoorLeafModel, updateDoorLeaf) — stays in DoorAnimationSystem
- `LevelBuilder` — already creates entities from placement data; will also create BehaviorComponent from declarations

### Established Patterns
- Components are POD structs (no methods, no inheritance) — BehaviorComponent follows this
- Systems inherit from `System` with init/update/shutdown — BehaviorSystem and TriggerSystem follow this
- Systems register with UpdatePhase for execution ordering — BehaviorSystem runs in Gameplay phase after InteractionSystem
- Free functions in RuntimeGameplay.cpp for actual logic — can keep this pattern for behavior handlers

### Integration Points
- `InteractionSystem::update()` → sets InteractionFocusState → BehaviorSystem reads this and dispatches
- `LevelDef::loadLevelDef()` → parser must emit BehaviorDeclaration structs per entity
- `LevelBuilder` → must attach BehaviorComponent to entities during build
- `GenericFileScene::registerScriptedGeometry("institutional_room", ...)` → must be updated or removed once behaviors are in .scene files
- Editor: EditorSceneDocument may need awareness of behavior data for serialization round-trip

</code_context>

<specifics>
## Specific Ideas

- The Source engine entity I/O system (from The Stanley Parable, the art reference) is the conceptual model: entities declare inputs/outputs, connections wire them with optional delays
- The institutional room scene (Phase 8) has 3 doors — open wooden door with warm light, locked gray metal door, locked white chained door — these are the primary migration test cases
- "self" as a special target resolves to the source entity, avoiding the need to repeat node IDs

</specifics>

<deferred>
## Deferred Ideas

- **ChangeScene action** — Scene transition as a behavior action; needs RuntimeGameSession integration
- **Lua scripting layer** — Research recommends deferring until behavior count exceeds ~30 or conditional branching is needed
- **Editor behavior inspector** — Full CRUD editing of behaviors in the inspector panel; read-only display acceptable for this phase
- **Visual scripting / node graph** — Explicitly not recommended for a solo dev project

</deferred>

---

*Phase: 13-data-driven-behavior-system*
*Context gathered: 2026-04-02*
