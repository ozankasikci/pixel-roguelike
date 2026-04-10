# Checkpoint Module Extraction Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extract the checkpoint feature into a self-contained module at `src/game/modules/checkpoint/`, following the same pattern as the door module.

**Architecture:** The checkpoint is currently scattered across `CheckpointComponent` (components/), `CheckpointSystem` (systems/), `spawnCheckpoint`/`spawnGameplayPrefab` (prefabs/), `RuntimeGameplay.cpp` (runtime checkpoint logic), and the archetype pipeline. We move all checkpoint-specific code into `src/game/modules/checkpoint/` with a single `registerCheckpointModule()` entry point that plugs into the keyword dispatch and behavior action handler registries — identical to how `registerDoorModule()` works.

**Tech Stack:** C++20, EnTT ECS, CMake, ImGui (editor inspector)

---

## Current checkpoint data flow

```
.prefab file → ContentRegistry (GameplayArchetypeDefinition)
    ↓
.scene file has "archetype_instance checkpoint_shrine ..."
    ↓
LevelDef.cpp parses into LevelArchetypePlacement
    ↓
LevelLoader calls ContentRegistry::findArchetype() → instantiateGameplayArchetype() → spawnGameplayPrefab() → spawnCheckpoint()
    ↓
spawnCheckpoint() creates: light entity + checkpoint root entity with CheckpointComponent + InteractableComponent
    ↓
Runtime: CheckpointSystem → updateRuntimeCheckpoints() in RuntimeGameplay.cpp
    ↓
Snapshot: RuntimeMutableSnapshot captures/restores CheckpointComponent state
```

## Target checkpoint data flow (after extraction)

```
.scene file has "checkpoint <name> x y z ..." (new first-class keyword, like door_group)
    ↓
CheckpointSerializer::parseCheckpoint() → LevelCheckpointPlacement in LevelDef
    ↓
LevelLoader calls spawnCheckpoint() from CheckpointSpawner
    ↓
CheckpointSpawner creates: light entity + root with CheckpointComponent + InteractableComponent + BehaviorComponent (ActivateCheckpoint action)
    ↓
Runtime: CheckpointAnimationSystem (module-owned, replaces CheckpointSystem)
    ↓
Snapshot: RuntimeMutableSnapshot still captures CheckpointComponent (unchanged)
```

## Key design decisions

1. **New scene keyword `checkpoint`** — replaces the `archetype_instance checkpoint_shrine` indirection. Checkpoints become a first-class entity like `door_group`, not a generic archetype. The archetype pipeline remains for other types.

2. **BehaviorComponent integration** — Checkpoints get a `BehaviorComponent` with `ActionType::ActivateCheckpoint` so activation flows through the same `InteractionSystem → BehaviorSystem → handler` pipeline as doors. This replaces the current custom activation logic in `updateRuntimeCheckpoints()`.

3. **Editor gets `EditorSceneObjectKind::Checkpoint`** — parallel to `DoorGroup`. The `ArchetypeInspector` pathway is preserved for non-checkpoint archetypes.

4. **Backward compatibility** — existing `archetype_instance checkpoint_shrine` lines in scene files continue to work through the existing archetype pipeline. New scenes use the `checkpoint` keyword. Migration is optional.

---

### Task 1: Create module directory and CMakeLists.txt

**Files:**
- Create: `src/game/modules/checkpoint/CMakeLists.txt`

**Step 1: Create the CMake target**

```cmake
# Checkpoint module — self-contained feature module
add_library(game_module_checkpoint STATIC
    CheckpointModule.cpp
    CheckpointSpawner.cpp
    CheckpointSerializer.cpp
    CheckpointSystem.cpp
    CheckpointActionHandler.cpp
)
target_include_directories(game_module_checkpoint PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_checkpoint PUBLIC
    engine_core
    engine_rendering
    gameplay
)
```

**Step 2: Wire into parent CMakeLists.txt**

Modify: `src/game/CMakeLists.txt:65`

Add after the `add_subdirectory(modules/door)` line:
```cmake
add_subdirectory(modules/checkpoint)
```

**Step 3: Commit**

```
Add checkpoint module CMake skeleton
```

---

### Task 2: Define LevelCheckpointPlacement and add to LevelDef

**Files:**
- Modify: `src/game/level/LevelDef.h`

**Step 1: Add the placement struct**

Add after `LevelDoorPlacement` (after line 126):

```cpp
struct LevelCheckpointPlacement {
    std::string name = "Checkpoint";
    glm::vec3 position{0.0f};
    glm::vec3 respawnPosition{0.0f};
    float interactDistance = 2.4f;
    float interactDotThreshold = 0.55f;
    glm::vec3 lightOffset{0.0f, 0.65f, -1.0f};
    glm::vec3 lightColor{1.0f, 0.7f, 0.42f};
    float lightRadius = 7.0f;
    float lightIntensity = 8.05f;
    std::string nodeId;
    std::string parentNodeId;
};
```

**Step 2: Add to LevelDef struct**

Add to `LevelDef` (after `doors` vector, line 139):

```cpp
std::vector<LevelCheckpointPlacement> checkpoints;
```

**Step 3: Commit**

```
Add LevelCheckpointPlacement to LevelDef
```

---

### Task 3: Create CheckpointSerializer (parse + serialize)

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointSerializer.h`
- Create: `src/game/modules/checkpoint/CheckpointSerializer.cpp`

**Step 1: Write the header**

```cpp
#pragma once

#include <string>
#include <vector>
#include <sstream>

struct LevelDef;

void parseCheckpoint(LevelDef& data,
                     const std::string& path,
                     int lineNumber,
                     const std::vector<std::string>& tokens);

void serializeCheckpoints(std::ostringstream& out, const LevelDef& data);
```

**Step 2: Write the implementation**

Scene format: `checkpoint <name> <x> <y> <z> [respawn <rx> <ry> <rz>] [interact_distance <d>] [interact_dot <d>] [light_offset <lx> <ly> <lz>] [light_color <r> <g> <b>] [light_radius <r>] [light_intensity <i>] [node <id>] [parent <id>]`

```cpp
#include "game/modules/checkpoint/CheckpointSerializer.h"

#include "game/content/ParseUtils.h"
#include "game/level/LevelDef.h"

#include <iomanip>
#include <stdexcept>

namespace {

bool tryParseFloatToken(const std::string& token, float& value) {
    std::size_t parsed = 0;
    try {
        value = std::stof(token, &parsed);
    } catch (const std::exception&) {
        return false;
    }
    return parsed == token.size();
}

bool parseNodeMetadata(const std::string& path, int lineNumber,
                       const std::vector<std::string>& tokens, std::size_t& index,
                       std::string& outNodeId, std::string& outParentNodeId) {
    if (tokens[index] == "node") {
        if (index + 1 >= tokens.size())
            throwParseError(path, lineNumber, "missing node id after 'node'");
        outNodeId = tokens[index + 1];
        index += 2;
        return true;
    }
    if (tokens[index] == "parent") {
        if (index + 1 >= tokens.size())
            throwParseError(path, lineNumber, "missing parent node id after 'parent'");
        outParentNodeId = tokens[index + 1];
        index += 2;
        return true;
    }
    return false;
}

std::string formatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
    return text;
}

void appendNodeMetadata(std::ostringstream& out,
                        const std::string& nodeId,
                        const std::string& parentNodeId) {
    if (!nodeId.empty()) {
        out << " node " << nodeId;
    }
    if (!parentNodeId.empty()) {
        out << " parent " << parentNodeId;
    }
}

} // namespace

void parseCheckpoint(LevelDef& data,
                     const std::string& path,
                     int lineNumber,
                     const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        throwParseError(path, lineNumber, "invalid checkpoint: need name x y z");
    }
    LevelCheckpointPlacement cp;
    cp.name = tokens[0];
    try {
        cp.position = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
    } catch (const std::exception&) {
        throwParseError(path, lineNumber, "invalid checkpoint position values");
    }
    std::size_t index = 4;
    while (index < tokens.size()) {
        if (tokens[index] == "respawn" && index + 3 < tokens.size()) {
            float rx, ry, rz;
            if (!tryParseFloatToken(tokens[index + 1], rx) ||
                !tryParseFloatToken(tokens[index + 2], ry) ||
                !tryParseFloatToken(tokens[index + 3], rz))
                throwParseError(path, lineNumber, "invalid respawn values");
            cp.respawnPosition = glm::vec3(rx, ry, rz);
            index += 4;
        } else if (tokens[index] == "interact_distance" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid interact_distance value");
            cp.interactDistance = val;
            index += 2;
        } else if (tokens[index] == "interact_dot" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid interact_dot value");
            cp.interactDotThreshold = val;
            index += 2;
        } else if (tokens[index] == "light_offset" && index + 3 < tokens.size()) {
            float lx, ly, lz;
            if (!tryParseFloatToken(tokens[index + 1], lx) ||
                !tryParseFloatToken(tokens[index + 2], ly) ||
                !tryParseFloatToken(tokens[index + 3], lz))
                throwParseError(path, lineNumber, "invalid light_offset values");
            cp.lightOffset = glm::vec3(lx, ly, lz);
            index += 4;
        } else if (tokens[index] == "light_color" && index + 3 < tokens.size()) {
            float r, g, b;
            if (!tryParseFloatToken(tokens[index + 1], r) ||
                !tryParseFloatToken(tokens[index + 2], g) ||
                !tryParseFloatToken(tokens[index + 3], b))
                throwParseError(path, lineNumber, "invalid light_color values");
            cp.lightColor = glm::vec3(r, g, b);
            index += 4;
        } else if (tokens[index] == "light_radius" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid light_radius value");
            cp.lightRadius = val;
            index += 2;
        } else if (tokens[index] == "light_intensity" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid light_intensity value");
            cp.lightIntensity = val;
            index += 2;
        } else if (parseNodeMetadata(path, lineNumber, tokens, index,
                                     cp.nodeId, cp.parentNodeId)) {
            // consumed
        } else {
            ++index;
        }
    }
    // Default respawn slightly in front of checkpoint if not specified
    if (cp.respawnPosition == glm::vec3(0.0f)) {
        cp.respawnPosition = cp.position + glm::vec3(0.0f, 1.6f, 2.5f);
    }
    data.checkpoints.push_back(std::move(cp));
}

void serializeCheckpoints(std::ostringstream& out, const LevelDef& data) {
    for (const auto& cp : data.checkpoints) {
        out << "checkpoint " << cp.name << ' '
            << formatFloat(cp.position.x) << ' '
            << formatFloat(cp.position.y) << ' '
            << formatFloat(cp.position.z);
        out << " respawn "
            << formatFloat(cp.respawnPosition.x) << ' '
            << formatFloat(cp.respawnPosition.y) << ' '
            << formatFloat(cp.respawnPosition.z);
        if (cp.interactDistance != 2.4f) out << " interact_distance " << formatFloat(cp.interactDistance);
        if (cp.interactDotThreshold != 0.55f) out << " interact_dot " << formatFloat(cp.interactDotThreshold);
        if (cp.lightOffset != glm::vec3(0.0f, 0.65f, -1.0f))
            out << " light_offset " << formatFloat(cp.lightOffset.x) << ' '
                << formatFloat(cp.lightOffset.y) << ' ' << formatFloat(cp.lightOffset.z);
        if (cp.lightColor != glm::vec3(1.0f, 0.7f, 0.42f))
            out << " light_color " << formatFloat(cp.lightColor.x) << ' '
                << formatFloat(cp.lightColor.y) << ' ' << formatFloat(cp.lightColor.z);
        if (cp.lightRadius != 7.0f) out << " light_radius " << formatFloat(cp.lightRadius);
        if (cp.lightIntensity != 8.05f) out << " light_intensity " << formatFloat(cp.lightIntensity);
        appendNodeMetadata(out, cp.nodeId, cp.parentNodeId);
        out << '\n';
    }
}
```

**Step 3: Commit**

```
Add CheckpointSerializer for checkpoint scene keyword
```

---

### Task 4: Add ActivateCheckpoint action type

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointActionTypes.h`
- Modify: `src/game/behavior/ActionTypes.h`

**Step 1: Create the action types header**

```cpp
#pragma once

// Checkpoint module action types.
// Included by ActionTypes.h to register checkpoint-specific enum values.

// No action params needed — activation is parameterless.
```

**Step 2: Add ActivateCheckpoint to ActionType enum**

Read `src/game/behavior/ActionTypes.h` and add `ActivateCheckpoint` to the `ActionType` enum (after the door action types). Also add `#include "game/modules/checkpoint/CheckpointActionTypes.h"`.

**Step 3: Commit**

```
Add ActivateCheckpoint action type
```

---

### Task 5: Create CheckpointActionHandler

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointActionHandler.h`
- Create: `src/game/modules/checkpoint/CheckpointActionHandler.cpp`

This handler replaces the activation logic currently in `updateRuntimeCheckpoints()`. When `ActivateCheckpoint` fires:
1. Set `CheckpointComponent.active = true`
2. Update `PlayerSpawnComponent.respawnPosition` on the player entity
3. Set `RuntimeCheckpointFeedbackState.messageTimer` for the busy text
4. Update `InteractableComponent` prompt

**Step 1: Write the header**

```cpp
#pragma once

#include <entt/entt.hpp>

struct ActionEntry;

void handleCheckpointAction(entt::registry& registry,
                            entt::entity source,
                            entt::entity target,
                            const ActionEntry& action);
```

**Step 2: Write the implementation**

```cpp
#include "game/modules/checkpoint/CheckpointActionHandler.h"

#include "game/behavior/ActionTypes.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/runtime/RuntimeGameplay.h"

void handleCheckpointAction(entt::registry& registry,
                            entt::entity source,
                            entt::entity target,
                            const ActionEntry& action) {
    (void)source;
    (void)action;

    auto* checkpoint = registry.try_get<CheckpointComponent>(target);
    if (!checkpoint || checkpoint->active) return;

    checkpoint->active = true;

    // Update player respawn position
    auto playerView = registry.view<PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, spawn] : playerView.each()) {
        (void)entity;
        spawn.respawnPosition = checkpoint->respawnPosition;
        break;
    }

    // Set feedback timer for busy text
    auto& ctx = registry.ctx();
    if (ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.get<RuntimeCheckpointFeedbackState>().messageTimer = 2.5f;
    }
}
```

**Step 3: Commit**

```
Add CheckpointActionHandler for ActivateCheckpoint
```

---

### Task 6: Create CheckpointSpawner

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointSpawner.h`
- Create: `src/game/modules/checkpoint/CheckpointSpawner.cpp`

This replaces `spawnCheckpoint()` in `GameplayPrefabs.cpp` for the new `checkpoint` keyword path. It spawns from `LevelCheckpointPlacement` (not `CheckpointSpawnSpec`).

**Step 1: Write the header**

```cpp
#pragma once

#include <entt/entt.hpp>

class LevelBuilder;
struct LevelCheckpointPlacement;

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement);
```

**Step 2: Write the implementation**

```cpp
#include "game/modules/checkpoint/CheckpointSpawner.h"

#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement) {
    auto& reg = builder.registry();

    // Spawn the checkpoint light
    auto lightEntity = builder.addLight(
        placement.position + placement.lightOffset,
        placement.lightColor,
        placement.lightRadius,
        placement.lightIntensity
    );

    // Create checkpoint root entity
    auto checkpoint = builder.createTransformEntity(placement.position);

    reg.emplace<CheckpointComponent>(
        checkpoint,
        CheckpointComponent{
            placement.respawnPosition,
            placement.interactDistance,
            placement.interactDotThreshold,
            false,
            lightEntity
        }
    );

    reg.emplace<InteractableComponent>(
        checkpoint,
        InteractableComponent{
            "E  KINDLE CHECKPOINT",
            "CHECKPOINT KINDLED",
            placement.interactDistance,
            placement.interactDotThreshold,
            true,
            false
        }
    );

    // Wire through BehaviorSystem like doors
    BehaviorComponent behavior;
    ActionEntry activateAction;
    activateAction.type = ActionType::ActivateCheckpoint;
    activateAction.targetNodeId = "self";
    activateAction.fireOnce = true;
    behavior.onActivate.push_back(activateAction);
    reg.emplace<BehaviorComponent>(checkpoint, behavior);

    if (!placement.nodeId.empty()) {
        builder.attachNodeId(checkpoint, placement.nodeId);
    }

    return checkpoint;
}
```

**Step 3: Commit**

```
Add CheckpointSpawner for checkpoint keyword entities
```

---

### Task 7: Create CheckpointSystem (module-owned)

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointSystem.h`
- Create: `src/game/modules/checkpoint/CheckpointSystem.cpp`

This replaces the old `CheckpointSystem` in `src/game/systems/`. The update logic moves from `updateRuntimeCheckpoints()` in `RuntimeGameplay.cpp` into this module. The new system only handles visual feedback (light intensity, prompt text, busy timer) — activation is handled by the action handler.

**Step 1: Write the header**

```cpp
#pragma once

#include <entt/entt.hpp>

// Free functions callable from RuntimeGameSession without Application&
void initializeCheckpointFeedback(entt::registry& registry);
void tickCheckpointFeedback(entt::registry& registry, float deltaTime);
```

**Step 2: Write the implementation**

```cpp
#include "game/modules/checkpoint/CheckpointSystem.h"

#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/TransformComponent.h"
#include "game/runtime/RuntimeGameplay.h"

#include <algorithm>

void initializeCheckpointFeedback(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
}

void tickCheckpointFeedback(entt::registry& registry, float deltaTime) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
    auto& feedback = ctx.get<RuntimeCheckpointFeedbackState>();

    if (feedback.messageTimer > 0.0f) {
        feedback.messageTimer = std::max(0.0f, feedback.messageTimer - deltaTime);
    }

    auto checkpointView = registry.view<TransformComponent, CheckpointComponent>();
    for (auto [entity, transform, checkpoint] : checkpointView.each()) {
        (void)transform;
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity)) {
            light->intensity = checkpoint.active ? 2.2f : 1.15f;
            light->radius = checkpoint.active ? 10.0f : 7.0f;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->promptText = checkpoint.active ? "RESPAWN ATTUNED" : "E  KINDLE CHECKPOINT";
            interactable->busyText = "CHECKPOINT KINDLED";
            interactable->busy = checkpoint.active && feedback.messageTimer > 0.0f;
        }
    }
}
```

**Step 3: Commit**

```
Add checkpoint feedback system to module
```

---

### Task 8: Create CheckpointModule registration entry point

**Files:**
- Create: `src/game/modules/checkpoint/CheckpointModule.h`
- Create: `src/game/modules/checkpoint/CheckpointModule.cpp`

**Step 1: Write the header**

```cpp
#pragma once

void registerCheckpointModule();
```

**Step 2: Write the implementation**

```cpp
#include "game/modules/checkpoint/CheckpointModule.h"

#include "game/modules/checkpoint/CheckpointActionHandler.h"
#include "game/modules/checkpoint/CheckpointSerializer.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorSystem.h"
#include "game/level/LevelDef.h"

void registerCheckpointModule() {
    registerLevelDefKeyword("checkpoint", parseCheckpoint, serializeCheckpoints);
    registerBehaviorActionHandler(ActionType::ActivateCheckpoint, handleCheckpointAction);
}
```

**Step 3: Commit**

```
Add registerCheckpointModule entry point
```

---

### Task 9: Wire checkpoint into LevelLoader

**Files:**
- Modify: `src/game/level/LevelLoader.cpp`

**Step 1: Add include**

Add after the door spawner include (line 7):
```cpp
#include "game/modules/checkpoint/CheckpointSpawner.h"
```

**Step 2: Add checkpoint spawning loop**

Add after the door group spawning loop (after line 92):
```cpp
    for (const auto& checkpointPlacement : level.checkpoints) {
        spawnCheckpointEntity(builder, checkpointPlacement);
    }
```

**Step 3: Commit**

```
Wire checkpoint module spawner into LevelLoader
```

---

### Task 10: Wire checkpoint into LevelDef hierarchy resolution

**Files:**
- Modify: `src/game/level/LevelDef.cpp`

The hierarchy resolution system in `resolveLevelHierarchy()` needs to know about checkpoint placements so they participate in the node graph (parent/child transforms). Find where `LevelNodeRef::Kind` enum is defined and add `Checkpoint`. Then add checkpoints to the `refs` vector building, the local transform lookup, and the resolved output — mirroring how archetypes and doors are handled.

Add checkpoints to:
1. `LevelNodeRef::Kind` enum — add `Checkpoint`
2. `refs` building loop — iterate `data.checkpoints` and push refs
3. `localTransformMatrix` switch — return transform from checkpoint position
4. Resolved output — write back resolved position to `resolved.checkpoints`
5. Entity count — add `data.checkpoints.size()` to total
6. Serialization — call registered keyword serializers (already happens via the registry)

**Step 1: Commit after all changes**

```
Add checkpoint to LevelDef hierarchy resolution
```

---

### Task 11: Update RuntimeGameSession to use module functions

**Files:**
- Modify: `src/game/runtime/RuntimeGameSession.cpp`

**Step 1: Replace checkpoint includes and calls**

Add include:
```cpp
#include "game/modules/checkpoint/CheckpointSystem.h"
```

Replace all calls to `initializeRuntimeCheckpoints(registry_)` with `initializeCheckpointFeedback(registry_)`.

Replace all calls to `updateRuntimeCheckpoints(registry_, deltaTime, runSession_)` and `updateRuntimeCheckpoints(registry_, 0.0f, runSession_)` with `tickCheckpointFeedback(registry_, deltaTime)` and `tickCheckpointFeedback(registry_, 0.0f)` respectively.

**Step 2: Commit**

```
RuntimeGameSession uses checkpoint module functions
```

---

### Task 12: Update runtime app to register checkpoint module

**Files:**
- Modify: `apps/runtime/main.cpp`

**Step 1: Add include and registration**

Add include:
```cpp
#include "game/modules/checkpoint/CheckpointModule.h"
```

Add after `registerDoorModule()` (line 89):
```cpp
    registerCheckpointModule();
```

**Step 2: Remove old CheckpointSystem registration**

The old `CheckpointSystem` (which was a `System` subclass) is registered at line 94. Remove that line and its include. The checkpoint feedback is now called directly from `RuntimeGameSession::tick()` via `tickCheckpointFeedback()` — no need for a separate system in the Application pipeline since `RuntimeGameSession` already calls it.

Remove from includes:
```cpp
#include "game/systems/CheckpointSystem.h"
```

Remove the system registration line:
```cpp
    auto& checkpoints = app.addSystem<CheckpointSystem>(Application::UpdatePhase::Interaction);
```

**Step 3: Commit**

```
Register checkpoint module in runtime app, remove old system
```

---

### Task 13: Update level editor to register checkpoint module

**Files:**
- Modify: `apps/level_editor/main.cpp`

**Step 1: Add include and registration**

Add include:
```cpp
#include "game/modules/checkpoint/CheckpointModule.h"
```

Add after `registerDoorModule()` (line 329):
```cpp
    registerCheckpointModule();
```

**Step 2: Commit**

```
Register checkpoint module in level editor
```

---

### Task 14: Remove old checkpoint code from RuntimeGameplay

**Files:**
- Modify: `src/game/runtime/RuntimeGameplay.h`
- Modify: `src/game/runtime/RuntimeGameplay.cpp`

**Step 1: Remove from header**

Remove these declarations from `RuntimeGameplay.h`:
```cpp
struct RuntimeCheckpointFeedbackState {
    float messageTimer = 0.0f;
};

void initializeRuntimeCheckpoints(entt::registry& registry);
void updateRuntimeCheckpoints(entt::registry& registry, float deltaTime, RunSession& session);
```

Move `RuntimeCheckpointFeedbackState` to a new header in the module: `src/game/modules/checkpoint/CheckpointFeedbackState.h`:

```cpp
#pragma once

struct RuntimeCheckpointFeedbackState {
    float messageTimer = 0.0f;
};
```

Update `RuntimeGameplay.h` to still declare the struct if other code references it, OR update the module's CheckpointSystem.h to include the new header instead of RuntimeGameplay.h.

**Step 2: Remove from implementation**

Remove `initializeRuntimeCheckpoints()` and `updateRuntimeCheckpoints()` functions from `RuntimeGameplay.cpp` (lines 273-315).

Remove the `ensureCheckpointFeedbackState()` helper (lines 90-96).

Remove the `#include "game/components/CheckpointComponent.h"` if no other function in the file uses it.

**Step 3: Commit**

```
Remove old checkpoint code from RuntimeGameplay
```

---

### Task 15: Remove old CheckpointSystem wrapper

**Files:**
- Delete: `src/game/systems/CheckpointSystem.h`
- Delete: `src/game/systems/CheckpointSystem.cpp`
- Modify: `src/game/CMakeLists.txt`

**Step 1: Remove from CMake**

Remove `systems/CheckpointSystem.cpp` from the `gameplay` target source list in `src/game/CMakeLists.txt`.

**Step 2: Delete the files**

Delete `src/game/systems/CheckpointSystem.h` and `src/game/systems/CheckpointSystem.cpp`.

**Step 3: Commit**

```
Remove old CheckpointSystem wrapper (replaced by module)
```

---

### Task 16: Add EditorSceneObjectKind::Checkpoint to editor

**Files:**
- Modify: `src/editor/scene/EditorSceneDocument.h`
- Modify: `src/editor/scene/EditorSceneDocument.cpp`

**Step 1: Add enum value and payload variant**

Add `Checkpoint` to `EditorSceneObjectKind` enum (after `DoorGroup`):
```cpp
    Checkpoint,
```

Add `LevelCheckpointPlacement` to the `EditorSceneObjectPayload` variant:
```cpp
using EditorSceneObjectPayload = std::variant<
    LevelMeshPlacement,
    LevelLightPlacement,
    LevelColliderPlacement,
    LevelReflectionProbePlacement,
    LevelPlayerSpawn,
    LevelArchetypePlacement,
    LevelGroupNode,
    LevelDoorPlacement,
    LevelCheckpointPlacement>;
```

Add `addCheckpoint()` method declaration:
```cpp
    std::uint64_t addCheckpoint(const LevelCheckpointPlacement& placement);
```

**Step 2: Implement in .cpp**

In `loadFromSceneFile()`, after loading doors, add:
```cpp
    for (const auto& checkpoint : level.checkpoints) {
        addCheckpoint(checkpoint);
    }
```

Add `addCheckpoint()` implementation:
```cpp
std::uint64_t EditorSceneDocument::addCheckpoint(const LevelCheckpointPlacement& placement) {
    return addObject(EditorSceneObjectKind::Checkpoint, placement);
}
```

In `toLevelDef()`, add a case for `LevelCheckpointPlacement`:
```cpp
            } else if constexpr (std::is_same_v<T, LevelCheckpointPlacement>) {
                level.checkpoints.push_back(p);
```

In `editorSceneObjectKindName()`, add:
```cpp
    case EditorSceneObjectKind::Checkpoint:
        return "Checkpoint";
```

In `editorSceneObjectLabel()`, add:
```cpp
        } else if constexpr (std::is_same_v<T, LevelCheckpointPlacement>) {
            label << " [" << p.name << "]";
```

In `editorSceneObjectAnchor()`, add:
```cpp
        } else if constexpr (std::is_same_v<T, LevelCheckpointPlacement>) {
            return p.position;
```

Handle the Checkpoint kind in all visitor lambdas that process `EditorSceneObjectPayload` — nodeIdPtr, parentNodeIdPtr, localTransformMatrix, supportsParenting, etc. Follow the exact pattern used for DoorGroup.

**Step 3: Commit**

```
Add Checkpoint as first-class editor scene object kind
```

---

### Task 17: Create CheckpointInspector for editor

**Files:**
- Create: `src/game/modules/checkpoint/editor/CheckpointInspector.h`
- Create: `src/game/modules/checkpoint/editor/CheckpointInspector.cpp`
- Modify: `src/editor/CMakeLists.txt`

**Step 1: Write the header**

```cpp
#pragma once

class EditorSceneDocument;
class EditorCommandStack;
struct EditorPendingCommand;
struct EditorSceneDocumentState;
struct LevelCheckpointPlacement;

void drawCheckpointInspector(LevelCheckpointPlacement& checkpoint,
                              EditorSceneDocument& document,
                              EditorCommandStack& commandStack,
                              EditorPendingCommand& pendingCommand,
                              const EditorSceneDocumentState& beforeState);
```

**Step 2: Write the implementation**

Follow the DoorGroupInspector pattern — render fields for position, respawn position, interact distance, interact dot, light offset, light color, light radius, light intensity. Use `trackLastItemCommand()` for undo/redo.

**Step 3: Add to editor CMakeLists.txt**

Add to the `editor` target source list (after the DoorGroupInspector line):
```
    ${CMAKE_SOURCE_DIR}/src/game/modules/checkpoint/editor/CheckpointInspector.cpp
```

**Step 4: Wire into SceneSelectionInspector**

Modify `src/editor/ui/inspectors/SceneSelectionInspector.cpp` to dispatch to `drawCheckpointInspector` when the selected object is `EditorSceneObjectKind::Checkpoint`.

**Step 5: Commit**

```
Add CheckpointInspector for editor scene objects
```

---

### Task 18: Wire checkpoint into EditorPreviewWorld

**Files:**
- Modify: `src/editor/scene/EditorPreviewWorld.cpp`

**Step 1: Add checkpoint case to spawnEntity**

In the switch on `object.kind`, add a case for `EditorSceneObjectKind::Checkpoint` that spawns the checkpoint light and root entity using the module spawner (or inline, mirroring how DoorGroup creates a minimal preview entity). The preview only needs the light and a transform entity — the gameplay components aren't needed for visual preview.

```cpp
    case EditorSceneObjectKind::Checkpoint: {
        auto& cp = std::get<LevelCheckpointPlacement>(object.payload);
        glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
        if (decomposeTransformMatrix(document.worldTransformMatrix(object.id), position, rotation, scale)) {
            cp.position = position;
        }
        auto lightEntity = builder.addLight(
            cp.position + cp.lightOffset,
            cp.lightColor,
            cp.lightRadius,
            cp.lightIntensity
        );
        auto root = builder.createTransformEntity(cp.position);
        registry_.emplace<CheckpointComponent>(root,
            CheckpointComponent{cp.respawnPosition, cp.interactDistance, cp.interactDotThreshold, false, lightEntity});
        break;
    }
```

**Step 2: Commit**

```
Wire checkpoint into editor preview world
```

---

### Task 19: Update CMake link dependencies

**Files:**
- Modify: `src/game/CMakeLists.txt`
- Modify: `apps/runtime/CMakeLists.txt` (if exists, or the main CMakeLists.txt)

**Step 1: Ensure executables link game_module_checkpoint**

Check where `game_module_door` is linked and add `game_module_checkpoint` alongside it. Look at the top-level or apps CMakeLists.txt.

**Step 2: Commit**

```
Link game_module_checkpoint in all executables
```

---

### Task 20: Build and verify

**Step 1: Build all three targets**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike level-editor procedural-model-viewer 2>&1 | tail -30
```

Fix any compilation errors.

**Step 2: Verify no references remain to old code**

Grep for `initializeRuntimeCheckpoints` and `updateRuntimeCheckpoints` — should only appear in the module or not at all. Grep for `spawnCheckpoint` in `GameplayPrefabs.cpp` — should still exist for backward-compatible archetype path.

**Step 3: Commit any fixes**

```
Fix build errors from checkpoint module extraction
```

---

### Task 21: Migrate one scene file to use checkpoint keyword

**Files:**
- Modify: `assets/scenes/initial_scene.scene` (or whichever scene currently uses `archetype_instance checkpoint_shrine`)

**Step 1: Add checkpoint keyword line**

Convert `archetype_instance checkpoint_shrine 0.0 0.0 -35.3 0.0` in `cathedral.scene` to:
```
checkpoint shrine 0.0 0.0 -35.3 respawn 0.0 1.6 -32.8 light_offset 0.0 0.65 -1.0 light_color 1.0 0.7 0.42 light_radius 7.0 light_intensity 8.05
```

Keep the `archetype_instance` line commented out for reference.

**Step 2: Build and test in editor**

Launch level editor, load the scene, verify checkpoint appears with light.

**Step 3: Commit**

```
Migrate cathedral checkpoint to first-class keyword
```

---

## Summary of module files

After all tasks, `src/game/modules/checkpoint/` contains:

```
src/game/modules/checkpoint/
├── CMakeLists.txt
├── CheckpointModule.h
├── CheckpointModule.cpp
├── CheckpointActionHandler.h
├── CheckpointActionHandler.cpp
├── CheckpointActionTypes.h
├── CheckpointFeedbackState.h
├── CheckpointSerializer.h
├── CheckpointSerializer.cpp
├── CheckpointSpawner.h
├── CheckpointSpawner.cpp
├── CheckpointSystem.h
├── CheckpointSystem.cpp
└── editor/
    ├── CheckpointInspector.h
    └── CheckpointInspector.cpp
```

## Files deleted

```
src/game/systems/CheckpointSystem.h
src/game/systems/CheckpointSystem.cpp
```

## Files modified

```
src/game/level/LevelDef.h          (add LevelCheckpointPlacement + vector)
src/game/level/LevelDef.cpp        (hierarchy resolution for checkpoints)
src/game/level/LevelLoader.cpp     (spawn checkpoint entities)
src/game/behavior/ActionTypes.h    (add ActivateCheckpoint)
src/game/runtime/RuntimeGameplay.h (remove checkpoint functions, move feedback state)
src/game/runtime/RuntimeGameplay.cpp (remove checkpoint functions)
src/game/runtime/RuntimeGameSession.cpp (use module functions)
src/game/CMakeLists.txt            (add_subdirectory, remove old system)
src/editor/CMakeLists.txt          (add inspector source)
src/editor/scene/EditorSceneDocument.h/cpp (add Checkpoint kind)
src/editor/scene/EditorPreviewWorld.cpp (checkpoint preview)
src/editor/ui/inspectors/SceneSelectionInspector.cpp (dispatch to inspector)
apps/runtime/main.cpp              (register module, remove old system)
apps/level_editor/main.cpp         (register module)
```
