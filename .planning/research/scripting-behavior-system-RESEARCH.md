# Scripting / Behavior System - Research

**Researched:** 2026-04-02
**Domain:** Entity behavior architecture for custom C++ ECS game engine
**Confidence:** HIGH

## Summary

The right approach for this project is a **two-tier hybrid architecture**: a native C++ "Action Component" system for the near term that evolves into an optional Lua scripting layer when iteration velocity demands it. The project currently has roughly a dozen behavior types (doors, checkpoints, triggers, pickups, levers, audio events, cutscenes) -- not thousands. This is a solo/small-team first-person horror game inspired by The Stanley Parable, which itself shipped on the Source engine's entity I/O system (a data-driven trigger/response architecture, not a full scripting language).

The existing codebase already has strong data-driven patterns (`.scene` files, `.environment` definitions, `ContentRegistry`, `GameplayArchetypeDefinition`) and a working `EventBus` with RAII subscription tokens. The immediate need is not "embed Lua" but rather "stop writing a new System class for every interactable behavior." The Source engine's I/O model (entities declare inputs/outputs, connections wire them together in data) maps directly onto the existing ECS + EventBus architecture and solves the stated problem with zero new dependencies.

**Primary recommendation:** Implement a native C++ Action/Response component system inspired by Source engine entity I/O. Wire behaviors through data-driven action lists on entities, dispatched by a single `BehaviorSystem`. Reserve Lua (via sol3 + LuaJIT) as a Phase 2 addition only if the number of unique behaviors exceeds ~30 or hot-reload becomes critical for level design iteration.

## Project Constraints (from CLAUDE.md)

### Engine and Build
- Custom C++ engine, no Unity/Unreal/Godot
- OpenGL 4.1 Core Profile (macOS ceiling)
- C++20 standard, CMake 3.28+ with FetchContent
- EnTT v3.16.0 for ECS
- Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`

### Naming and Style
- Components are POD structs (no methods, no inheritance)
- Systems inherit from `System` base class with `init()`, `update()`, `shutdown()`
- Classes: PascalCase; functions: camelCase; variables: snake_case; constants: kPascalCase
- Files: PascalCase.h / PascalCase.cpp

### Architecture
- Three layers: Engine > Game > Editor
- Systems execute in phase order: Input > Interaction > Physics > Gameplay > Camera > Render
- Services via type-safe service locator on Application
- EventBus for type-safe pub/sub (already exists, RAII tokens)

## Standard Stack

### Phase 1: Native C++ (Recommended Now)

No new libraries required. Uses existing EnTT, EventBus, and ContentRegistry.

| Component | Already In Project | Purpose |
|-----------|-------------------|---------|
| EnTT v3.16.0 | Yes | ECS registry, views, component queries |
| EventBus | Yes | Type-safe pub/sub for entity communication |
| ContentRegistry | Yes | Data-driven definition loading |
| entt::meta | Yes (part of EnTT) | Runtime reflection for type-erased component access |

### Phase 2: Lua Scripting (Future, If Needed)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua | 5.4.8 (stable) or LuaJIT 2.1 | Scripting runtime | Industry standard for game scripting; 30+ years of game industry use; tiny footprint (~250KB); LuaJIT is 10-100x faster than standard Lua |
| sol3 | v3.3.0 (Jun 2024) | C++ <-> Lua binding | Best-in-class C++ Lua wrapper; header-only; supports all Lua 5.1+ and LuaJIT; top performance |
| entt-meets-sol2 | CC0 reference | Integration pattern reference | Demonstrates registry/dispatcher/scheduler exposure to Lua; not a library to vendor, but a pattern to follow |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Lua + sol3 | AngelScript (v2.38.0, Aug 2025) | C++-like syntax, strong typing, excellent C++ binding; heavier than Lua; less ecosystem/community; good if scripters are C++ programmers |
| Lua + sol3 | Wren | Lightweight, class-based; **maintenance concerns** -- project underwent leadership transfer, unclear activity; smaller community |
| Lua + sol3 | ChaiScript | Header-only, C++-like; **significantly slower** than Lua/LuaJIT; not suitable for per-frame calls |
| Native Action System | Full Lua from day one | Premature complexity for ~12 behavior types; Lua adds build dependency, debugging complexity, binding maintenance; not needed until behavior count grows significantly |

### What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| ChaiScript for gameplay | Performance is 5-10x slower than Lua; unsuitable for per-frame entity updates | Lua + LuaJIT if scripting needed |
| Wren | Uncertain maintenance; small ecosystem; leadership transition | Lua (decades of stability) |
| Python embedding | GIL, heavy runtime, poor real-time performance | Lua |
| Full visual scripting (node graph) | Enormous implementation effort for a solo dev; overkill for this scope | Data-driven action lists in .scene files |

## Architecture Patterns

### Recommended Project Structure

```
src/
├── engine/
│   └── core/
│       └── EventBus.h              # Already exists
├── game/
│   ├── behavior/
│   │   ├── ActionTypes.h           # Enum of built-in action types
│   │   ├── BehaviorComponent.h     # Data: list of actions per entity
│   │   ├── BehaviorSystem.h/cpp    # Single system dispatches all behaviors
│   │   ├── TriggerComponent.h      # Data: trigger volumes and conditions
│   │   └── TriggerSystem.h/cpp     # Detects trigger conditions, fires events
│   ├── components/
│   │   └── InteractableComponent.h # Already exists (keep)
│   └── systems/
│       └── InteractionSystem.h/cpp # Already exists (keep, becomes trigger source)
```

### Pattern 1: Action Component (Source Engine I/O Inspired)

**What:** Each entity carries a `BehaviorComponent` containing a list of actions (responses) that fire when the entity receives an activation event. Actions are enum-typed with associated parameter data. A single `BehaviorSystem` iterates entities with pending activations and executes their action lists.

**When to use:** For all entity behaviors that respond to player interaction, trigger volumes, or other entity activations. Covers doors, levers, pickups, audio triggers, light toggles, cutscene triggers, locks, environmental puzzles.

**Why this matches the project:** The Stanley Parable (the art reference) shipped on Source engine, which uses exactly this entity I/O pattern. The game's behaviors are trigger-response chains: player interacts with door -> door opens -> light flickers -> narrator speaks. This is not AI or procedural generation -- it is authored, deterministic cause-and-effect.

**Example:**

```cpp
// ActionTypes.h
enum class ActionType : uint8_t {
    OpenDoor,           // Animate door open (params: duration, angle)
    CloseDoor,          // Animate door closed
    ToggleDoor,         // Open if closed, close if open
    PlaySound,          // Play audio (params: sound_id, volume, spatial)
    SetLight,           // Change light properties (params: intensity, color, radius)
    FlickerLight,       // Flicker light for duration (params: duration, rate)
    EnableEntity,       // Enable an entity's interactable
    DisableEntity,      // Disable an entity's interactable
    TeleportPlayer,     // Move player to position (params: target_pos)
    ShowMessage,        // Display on-screen text (params: text, duration)
    TriggerCheckpoint,  // Activate checkpoint behavior
    ChangeScene,        // Load a new scene (params: scene_id)
    Delay,              // Wait before next action (params: seconds)
    EmitEvent,          // Publish a named event on EventBus
    LockPlayer,         // Lock player movement for duration
    UnlockPlayer,       // Release player movement lock
};

// BehaviorComponent.h
struct ActionEntry {
    ActionType type;
    std::string targetNodeId;           // Which entity receives this action
    float delay = 0.0f;                 // Seconds before action fires
    bool fireOnce = false;              // Only fire once, then disable

    // Variant parameter block (union or std::variant for POD)
    float paramFloat1 = 0.0f;          // duration, intensity, angle, etc.
    float paramFloat2 = 0.0f;
    float paramFloat3 = 0.0f;
    std::string paramString;            // sound_id, message text, scene_id
    glm::vec3 paramVec3{0.0f};         // position, color
    bool fired = false;                 // Tracks fire-once state
};

struct BehaviorComponent {
    std::vector<ActionEntry> onActivate;   // When entity is interacted with
    std::vector<ActionEntry> onEnter;      // When player enters trigger volume
    std::vector<ActionEntry> onExit;       // When player exits trigger volume
    std::vector<ActionEntry> onTimer;      // When timer expires
    bool enabled = true;
};
```

### Pattern 2: Trigger Volume Component

**What:** Entities with a `TriggerComponent` define a spatial volume. The `TriggerSystem` checks player overlap each frame and fires activation events when the player enters or exits.

**When to use:** For proximity-based behaviors: entering a room triggers audio, stepping on a plate opens a door, walking through a hallway triggers a cutscene.

```cpp
// TriggerComponent.h
enum class TriggerShape : uint8_t { Box, Sphere };

struct TriggerComponent {
    TriggerShape shape = TriggerShape::Box;
    glm::vec3 halfExtents{1.0f};        // For box triggers
    float radius = 1.0f;                 // For sphere triggers
    bool fireOnce = false;
    bool playerInside = false;           // Runtime state
    bool enabled = true;
};
```

### Pattern 3: Behavior System as Single Dispatcher

**What:** Instead of DoorSystem, CheckpointSystem, etc., a single `BehaviorSystem` processes all `BehaviorComponent` action lists. Specific action types map to handler functions.

```cpp
// BehaviorSystem.cpp -- dispatch sketch
void BehaviorSystem::executeAction(entt::registry& registry,
                                    entt::entity source,
                                    const ActionEntry& action,
                                    float deltaTime) {
    entt::entity target = resolveTarget(registry, source, action.targetNodeId);

    switch (action.type) {
    case ActionType::OpenDoor:
        if (auto* door = registry.try_get<DoorComponent>(target)) {
            door->opening = true;
            door->openDuration = action.paramFloat1;
        }
        break;
    case ActionType::PlaySound:
        if (auto* audio = registry.try_get<AudioSourceComponent>(target)) {
            audio->soundId = action.paramString;
            audio->play = true;
        }
        break;
    case ActionType::SetLight:
        if (auto* light = registry.try_get<LightComponent>(target)) {
            light->intensity = action.paramFloat1;
            light->color = action.paramVec3;
        }
        break;
    case ActionType::ShowMessage:
        // Publish on EventBus for UI system to consume
        app_.eventBus().publish(ShowMessageEvent{action.paramString, action.paramFloat1});
        break;
    // ... other action handlers
    }
}
```

### Pattern 4: Data-Driven Behavior in .scene Files

**What:** Extend the existing `.scene` file format to declare behaviors inline with entity placement.

```
# A door that opens and plays a sound when interacted
mesh wood_door 0.0 0.0 5.95 1.0 1.0 1.0 0.0 0.0 0.0 material wood_door_1 node n_door_a
  interactable "E  Open door" distance 2.0
  on_activate open_door duration 1.2 target self
  on_activate play_sound "door_creak" target self delay 0.1
  on_activate set_light intensity 2.0 color 1.0 0.9 0.7 target n_light_a delay 0.5

# A trigger volume that fires when player enters
trigger_box 0.0 1.0 3.0 2.0 2.0 2.0 node n_trigger_hallway
  on_enter play_sound "ambient_whisper" target self fire_once
  on_enter show_message "You feel a chill..." duration 3.0 fire_once
```

### Pattern 5: Phase 2 -- Lua ScriptComponent (Future)

**What:** For behaviors too complex to express as action lists (branching logic, stateful puzzles, dialogue trees), add a `ScriptComponent` that holds a Lua table with lifecycle hooks.

**When to use:** Only when:
- Unique behavior count exceeds ~30 and action enums become unwieldy
- You need conditional branching (if player has key X, then open door Y)
- Hot-reload during level design iteration becomes critical
- Cutscene sequencing needs more than linear action chains

```cpp
// ScriptComponent.h (Phase 2 only)
struct ScriptComponent {
    std::string scriptPath;             // "scripts/entities/puzzle_lock.lua"
    sol::table instance;                // Lua table with self-reference
    struct {
        sol::function onActivate;
        sol::function onUpdate;
        sol::function onEnter;
        sol::function onExit;
    } hooks;
};
```

```lua
-- scripts/entities/puzzle_lock.lua (Phase 2 only)
local PuzzleLock = {}

function PuzzleLock:onActivate(activator)
    if self.locked then
        registry:get(self.entity, "InteractableComponent").promptText =
            "This lock requires the " .. self.requiredKey
        return
    end
    registry:get(self.entity, "DoorComponent").opening = true
end

function PuzzleLock:onUpdate(dt)
    -- Stateful timer logic, etc.
end

return PuzzleLock
```

### Anti-Patterns to Avoid

- **One System per behavior type:** The current pattern (DoorSystem, CheckpointSystem, etc.) does not scale. Each new interactable requires a new Component + System + registration. Use a single BehaviorSystem dispatching action lists instead.
- **Embedding Lua for simple trigger-response:** Lua is heavyweight for "press E -> door opens -> sound plays." Action lists handle this declaratively.
- **Virtual dispatch behavior hierarchies:** Do not create `class Behavior { virtual void onActivate(); }` with `DoorBehavior`, `LeverBehavior` subclasses. This is inheritance-over-composition and fights the ECS architecture.
- **String-based action dispatch:** Do not use `std::string` action type names with `if/else` chains. Use an `enum class` for compile-time safety and switch dispatch.
- **Storing Lua state in components for serialization:** Lua tables are not trivially serializable. Keep BehaviorComponent as POD for save/load compatibility. ScriptComponent should reconstruct from file path.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| C++ <-> Lua binding | Manual Lua C API calls with lua_push/lua_to | sol3 (v3.3.0) | sol3 handles type conversion, error handling, coroutines, usertype registration; the Lua C API is error-prone and verbose |
| Type-erased component access from scripts | Custom RTTI or type maps | EnTT's built-in `entt::meta` reflection system | EnTT already has a complete non-intrusive reflection system with `meta_factory`, `meta_any`, runtime type resolution -- use it |
| Event pub/sub | Custom observer pattern | Existing `EventBus` | Already implemented with RAII tokens, type-safe dispatch, and works correctly |
| Entity targeting by name | Linear search through all entities | Index: `std::unordered_map<std::string, entt::entity>` maintained by LevelBuilder | Node IDs already exist in `.scene` files (`node n_door_a`); just build a lookup table at load time |
| Delayed action scheduling | Custom timer lists | Priority queue or sorted vector of pending actions in BehaviorSystem | Simple `{fire_time, entity, action_index}` struct sorted by time; pop and execute when `fire_time <= current_time` |
| Lua hot-reload | Custom file watcher | `std::filesystem::last_write_time` polling (same pattern as existing `ContentRegistry::pollMaterialHotReload`) | The project already polls material file modification times; reuse this exact pattern for Lua scripts |

**Key insight:** The project already has 80% of the infrastructure for a behavior system. The missing piece is a generic action dispatch layer on top of the existing InteractionSystem -> InteractionFocusState -> per-system consumption pattern. The current flow (InteractionSystem detects focus -> sets `activationRequested` -> DoorSystem/CheckpointSystem checks and consumes) should become: InteractionSystem detects focus -> sets `activationRequested` -> BehaviorSystem reads BehaviorComponent action list -> dispatches actions.

## Common Pitfalls

### Pitfall 1: Premature Scripting Language Integration
**What goes wrong:** Developer adds Lua before having enough behaviors to justify it. Now every behavior requires both C++ binding code AND a Lua script. Development slows down.
**Why it happens:** "Real engines have scripting" mindset. Lua seems easy to add.
**How to avoid:** Count your unique behavior types. Below ~20-30, native action lists are faster to implement and debug. Add Lua only when the action enum becomes unwieldy or when conditional logic is needed.
**Warning signs:** More time writing bindings than writing gameplay logic.

### Pitfall 2: One System Per Interactable Type
**What goes wrong:** Every new interactable (lever, pickup, terminal, switch) requires a new Component type, a new System class, new `initializeRuntime` / `updateRuntime` functions, new header includes, and updating the system registration in Application setup.
**Why it happens:** The existing pattern (DoorSystem, CheckpointSystem) makes this the path of least resistance.
**How to avoid:** Create a single BehaviorSystem that dispatches actions based on data. New behavior types are new `ActionType` enum values + handler functions, not new System classes.
**Warning signs:** `src/game/systems/` directory growing with many small, similar files.

### Pitfall 3: Action Parameter Explosion
**What goes wrong:** The `ActionEntry` struct grows to have 20+ optional fields for different action types. Most fields are unused for any given action.
**Why it happens:** Trying to make one struct fit all action types.
**How to avoid:** Use `std::variant` for action-specific parameter blocks, or use a small set of generic parameter slots (3 floats, 1 string, 1 vec3 covers most cases). Keep it simple -- this is a horror puzzle game, not a scripting language.
**Warning signs:** ActionEntry has more than ~8 data fields.

### Pitfall 4: Forgetting Entity Lifetime and Target Resolution
**What goes wrong:** An action references a target entity by stored `entt::entity` value. The target entity is destroyed and recreated (e.g., scene reload). The stored entity ID now points to nothing or a different entity.
**Why it happens:** Using entity IDs as stable references across scene boundaries.
**How to avoid:** Target entities by node ID string (`targetNodeId`), resolved at dispatch time via a name -> entity lookup table. This is how Source engine's `targetname` system works.
**Warning signs:** "Ghost" behaviors firing on wrong entities after scene transitions.

### Pitfall 5: Lua GC Pauses During Gameplay
**What goes wrong:** Lua's garbage collector runs a full collection mid-frame, causing a visible hitch.
**Why it happens:** Default Lua GC is generational but can still cause pauses when many objects are allocated.
**How to avoid:** (Phase 2 only) Use `lua_gc(L, LUA_GCINC, ...)` to run incremental GC. Budget ~0.5ms per frame for GC. Pool Lua tables instead of creating new ones each frame.
**Warning signs:** Irregular frame time spikes that correlate with Lua script density.

### Pitfall 6: Over-Exposing Engine Internals to Scripts
**What goes wrong:** Lua scripts can call any C++ function, modify any component, break invariants.
**Why it happens:** Binding everything "for convenience."
**How to avoid:** Expose a narrow, validated API facade: `entity:openDoor()`, `entity:playSound(id)`, `entity:setLightIntensity(val)`. Do not expose raw `registry:emplace()` or direct component field mutation.
**Warning signs:** Lua scripts directly modifying physics state, OpenGL resources, or system pointers.

## Code Examples

### Example 1: BehaviorComponent in .scene File (Data-Driven)

```
# Scene file syntax extension (compatible with existing parser pattern)
mesh wood_door 0.0 0.0 5.95 1.0 1.0 1.0 0.0 0.0 0.0 material wood_door_1 node n_door_a
  interactable "E  Open door" distance 2.0
  behavior on_activate open_door target self duration 1.2
  behavior on_activate play_sound target self sound door_creak delay 0.1
  behavior on_activate set_light target n_lamp_01 intensity 2.0 delay 0.5
```

### Example 2: Entity Name Resolution at Load Time

```cpp
// NodeIndex.h -- maintained by LevelBuilder during scene load
struct NodeIndex {
    std::unordered_map<std::string, entt::entity> byNodeId;

    entt::entity resolve(const std::string& nodeId) const {
        auto it = byNodeId.find(nodeId);
        return it != byNodeId.end() ? it->second : entt::null;
    }

    entt::entity resolve(const std::string& nodeId, entt::entity self) const {
        if (nodeId == "self" || nodeId.empty()) return self;
        return resolve(nodeId);
    }
};
```

### Example 3: Delayed Action Queue

```cpp
// PendingAction.h
struct PendingAction {
    float fireTime;                  // Absolute time to fire
    entt::entity source;
    size_t actionIndex;              // Index into BehaviorComponent::onActivate
    std::string eventType;           // "activate", "enter", "exit"
};

// In BehaviorSystem, maintained as sorted vector:
std::vector<PendingAction> pending_actions_;

void BehaviorSystem::update(Application& app, float deltaTime) {
    float current_time = app.time().elapsed();

    // Process due actions
    while (!pending_actions_.empty() && pending_actions_.front().fireTime <= current_time) {
        auto action = pending_actions_.front();
        pending_actions_.erase(pending_actions_.begin());

        if (!app.registry().valid(action.source)) continue;
        auto* behavior = app.registry().try_get<BehaviorComponent>(action.source);
        if (!behavior || !behavior->enabled) continue;

        auto& list = getActionList(*behavior, action.eventType);
        if (action.actionIndex < list.size()) {
            executeAction(app.registry(), action.source, list[action.actionIndex], deltaTime);
        }
    }

    // Check for new activations from InteractionFocusState
    processNewActivations(app);

    // Check trigger volumes
    processTriggerOverlaps(app);
}
```

### Example 4: Migration Path from Current DoorSystem

```cpp
// Before (current): DoorSystem consumes InteractionFocusState directly
// After: BehaviorSystem reads BehaviorComponent, calls existing door logic

// The DoorComponent and its animation logic stay unchanged.
// What changes is HOW the door gets activated:

// Old: DoorSystem::update checks focus.focused, checks DoorComponent, sets opening=true
// New: BehaviorSystem::executeAction(ActionType::OpenDoor) sets door.opening=true

// The animation update (progress tracking, leaf rotation) can either:
// (a) Stay in a slimmed-down DoorAnimationSystem (only animates, never activates)
// (b) Move into BehaviorSystem's per-frame update for active animations
// Option (a) is recommended: keep animation concerns separate from activation.
```

### Example 5: Phase 2 Lua Setup (For Reference Only)

```cmake
# CMakeLists.txt addition (Phase 2 only)
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.8
)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(lua sol2)

target_link_libraries(gameplay PRIVATE sol2::sol2 lua::lua)
```

```cpp
// ScriptingSystem.cpp (Phase 2 only)
void ScriptingSystem::init(Application& app) {
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // Expose narrow API facade -- NOT raw registry access
    lua_.new_usertype<EntityHandle>("Entity",
        "openDoor", &EntityHandle::openDoor,
        "playSound", &EntityHandle::playSound,
        "setLight", &EntityHandle::setLight,
        "showMessage", &EntityHandle::showMessage,
        "hasComponent", &EntityHandle::hasComponent,
        "position", sol::property(&EntityHandle::getPosition)
    );
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| One System per behavior | Data-driven action lists dispatched by single system | Standard practice in commercial engines since Source (2004) | Dramatically reduces boilerplate for new behaviors |
| Lua as primary scripting | Lua/LuaJIT as optional gameplay layer, not mandatory | 2020s indie engines | Only embed when complexity justifies it |
| Custom Lua C API binding | sol3 wrapper (v3.3.0) | sol2 first release 2016, sol3 stable 2024 | Eliminates manual stack manipulation; type-safe |
| Manual type registration for scripts | EnTT meta reflection system | Available since EnTT v3.x | Non-intrusive, macro-free runtime reflection built into EnTT |
| Lua 5.3/5.4 interpreter only | LuaJIT 2.1 for hot paths | LuaJIT has been standard for games for years | 10-100x perf improvement over standard Lua interpreter |
| Wren as Lua alternative | Wren maintenance uncertain | 2023+ leadership transfer | Lua remains the safe choice for new projects |
| AngelScript niche usage | AngelScript v2.38.0 (Aug 2025) actively maintained | Recent major update | Viable alternative if C++-like syntax preferred; Hazelight ships games with it (It Takes Two, Split Fiction) |

**Deprecated/outdated:**
- ChaiScript: Performance unsuitable for game use; rarely chosen in modern engines
- Wren: Uncertain maintenance trajectory; avoid for new projects
- Manual Lua C API without wrapper: No reason to do this when sol3 exists

## Industry Context

### Source Engine Entity I/O (Most Relevant Reference)
The Stanley Parable (the project's art reference) shipped on Source engine's entity I/O system. Entities declare **outputs** (events that fire on state changes) and **inputs** (actions that can be invoked). **Connections** wire outputs to inputs with optional delays and parameter overrides. Key entity types include `trigger_once`, `trigger_multiple`, `logic_relay` (output forwarder), `logic_timer`, and `logic_auto` (fires on map load). This system powers complex interactive levels without any embedded scripting language. Source: [Valve Developer Community - Inputs and Outputs](https://developer.valvesoftware.com/wiki/Inputs_and_Outputs).

### Godot Signal System
Godot implements the observer pattern through its signal system. Signals are first-class types (Godot 4.0+), enabling type-safe, loosely-coupled communication between nodes. Nodes emit signals when events occur; other nodes connect handlers. This maps directly to the EventBus + BehaviorComponent pattern. Source: [Godot Docs - Signals](https://docs.godotengine.org/en/stable/getting_started/step_by_step/signals.html).

### Bevy (Rust ECS) Approach
Bevy uses systems as the scripting layer -- gameplay logic is Rust functions operating on component queries. Complex AI uses behavior tree plugins (`bevy_behave`). Bevy explicitly does not ship a scripting language; the ECS IS the behavior system. This validates the "native first, scripting optional" approach. Source: [Bevy ECS Guide](https://bevy.org/learn/quick-start/getting-started/ecs/).

### EnTT + Lua Integration (entt-meets-sol2)
The reference implementation exposes EnTT registry, dispatcher, and scheduler to Lua via sol2. The `ScriptComponent` pattern stores a `sol::table` with lifecycle hooks (`init`, `update`, `destroy`). Runtime views enable dynamic component queries from Lua. CC0 licensed. Source: [entt-meets-sol2 GitHub](https://github.com/skaarj1989/entt-meets-sol2).

## Open Questions

1. **Action parameter representation**
   - What we know: A small set of generic parameter slots (3 floats, 1 string, 1 vec3) covers most cases. `std::variant` is an alternative.
   - What's unclear: Whether the parameter set is sufficient for all planned puzzle types.
   - Recommendation: Start with the generic slots. Add `std::variant<DoorActionParams, SoundActionParams, LightActionParams>` only if type safety at parse time becomes important.

2. **Scene file format extension**
   - What we know: The current `.scene` parser is line-based with keyword prefixes. Adding `behavior` lines and `trigger_box` is straightforward.
   - What's unclear: Whether nested/indented syntax (as shown in examples) would require parser refactoring.
   - Recommendation: Investigate the current `LevelLoader` parser. If it supports continuation lines or sub-entity parsing (as `archetype` entries suggest), extend it. Otherwise, use flat `behavior_` prefixed lines.

3. **Editor integration**
   - What we know: The level editor already has an inspector panel, outliner, and object properties.
   - What's unclear: How behavior action lists would be displayed and edited in the editor UI.
   - Recommendation: Defer editor UI for behaviors to a later phase. Start with hand-edited `.scene` files. The editor can display behaviors as read-only initially.

4. **Lua 5.4 vs LuaJIT compatibility with sol3**
   - What we know: sol3 v3.3.0 supports Lua 5.1-5.4 and LuaJIT 2.0+. LuaJIT is faster but stuck at Lua 5.1 semantics. Lua 5.4 has integers and generational GC.
   - What's unclear: Whether LuaJIT's 5.1 limitation would matter for this project's scripting needs.
   - Recommendation: Use LuaJIT for runtime, standard Lua 5.4 for development/debugging. sol3 abstracts the difference.

## Metadata

**Confidence breakdown:**
- Standard stack (Phase 1 native): HIGH - Uses only existing project libraries; pattern well-established in commercial engines
- Standard stack (Phase 2 Lua): HIGH - sol3 + Lua is the most proven C++ game scripting combination; versions verified from official sources
- Architecture patterns: HIGH - Source engine I/O pattern is decades-proven; directly applicable to this game type
- Pitfalls: HIGH - Based on documented industry experience and analysis of the existing codebase's pain points
- Migration path: MEDIUM - Specific implementation details depend on current parser structure and editor capabilities

**Research date:** 2026-04-02
**Valid until:** 2026-07-02 (90 days -- these are stable, mature technologies)

## Sources

### Primary (HIGH confidence)
- [EnTT Runtime Reflection System](https://skypjack.github.io/entt/md_docs_2md_2meta.html) - meta_factory, meta_any, runtime type registration
- [EnTT Wiki - Bridging Lua and EnTT](https://github.com/skypjack/entt/issues/435) - EnTT author guidance on scripting integration
- [Valve Developer Community - Inputs and Outputs](https://developer.valvesoftware.com/wiki/Inputs_and_Outputs) - Source engine entity I/O architecture
- [Valve Dev Union - Entity Interactions Guide](https://valvedev.info/guides/entity-interactions-in-sources-input-output-system/) - Detailed I/O system guide
- [sol3 Documentation](https://sol2.readthedocs.io/en/latest/) - sol 3.2.3 docs, build instructions
- [sol2 GitHub Releases](https://github.com/thephd/sol2/releases) - v3.3.0 confirmed (Jun 2024)
- [Lua Official Site](https://www.lua.org/versions.html) - Lua 5.4.8 (Jun 2025), Lua 5.5.0 (Dec 2025)
- [entt-meets-sol2](https://github.com/skaarj1989/entt-meets-sol2) - Reference implementation for EnTT + sol2 + Lua

### Secondary (MEDIUM confidence)
- [AngelScript Official](https://www.angelcode.com/angelscript/) - v2.38.0 (Aug 2025), active development confirmed
- [Godot Signals Documentation](https://docs.godotengine.org/en/stable/getting_started/step_by_step/signals.html) - Signal system architecture
- [That One Game Dev - Lua with sol2](https://thatonegamedev.com/cpp/introduction-to-lua-in-c-with-sol2/) - Practical sol2 tutorial
- [Tea-Age Solutions - Scripting Language Comparison](https://tea-age.solutions/2023/01/31/script-language-comparison-for-embed-in-cpp/) - Performance benchmarks (Lua, AngelScript, Wren, ChaiScript)
- [mode13h.dev - Lua Performance](https://mode13h.dev/a-case-for-lua-performance/) - LuaJIT performance characteristics
- [Bevy ECS Guide](https://bevy.org/learn/quick-start/getting-started/ecs/) - Systems as behavior layer

### Tertiary (LOW confidence)
- [GameDev.net - Lua in ECS](https://www.gamedev.net/forums/topic/715511-how-to-use-lua-in-a-c-gameengine-based-on-ecs/) - Community discussion, various approaches
- [Behavior Trees vs FSMs paper](https://arxiv.org/abs/2405.16137) - Academic comparison (May 2024)
