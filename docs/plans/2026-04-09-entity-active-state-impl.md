# Entity Active State System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Unity-style enable/disable for entities with hierarchical propagation and automatic system filtering.

**Architecture:** GameRegistry wrapper owns entt::registry; its `view()` auto-excludes `DisabledTag`. Runtime parent-child hierarchy built from existing parentNodeId data during level loading. `setEntityActive()` propagates disable/enable through the hierarchy tree.

**Tech Stack:** C++20, EnTT v3.16, CMake

**Design doc:** `docs/plans/2026-04-09-entity-active-state-design.md`

---

### Task 1: Create ECS component headers

New engine-level components for the active state system. All are in `src/engine/ecs/` so GameRegistry can reference DisabledTag without engine→game dependency.

**Files:**
- Create: `src/engine/ecs/DisabledTag.h`
- Create: `src/engine/ecs/ActiveStateComponent.h`
- Create: `src/engine/ecs/HierarchyComponents.h`

**Step 1: Create DisabledTag.h**

```cpp
#pragma once

// Zero-size marker component. When present on an entity, GameRegistry::view()
// automatically excludes it from query results. This is the ECS equivalent of
// Unity's activeInHierarchy == false.
struct DisabledTag {};
```

**Step 2: Create ActiveStateComponent.h**

```cpp
#pragma once

// Tracks whether an entity was explicitly disabled (activeSelf) vs inherited
// from a disabled parent. Only added to entities that have been explicitly
// toggled at least once — absence implies activeSelf == true.
struct ActiveStateComponent {
    bool activeSelf = true;
};
```

**Step 3: Create HierarchyComponents.h**

```cpp
#pragma once

#include <entt/entt.hpp>
#include <vector>

// Runtime parent reference. Built during level loading from parentNodeId data.
struct ParentComponent {
    entt::entity parent = entt::null;
};

// Runtime children list. Built during level loading alongside ParentComponent.
struct ChildrenComponent {
    std::vector<entt::entity> children;
};
```

**Step 4: Commit**

```bash
git add src/engine/ecs/DisabledTag.h src/engine/ecs/ActiveStateComponent.h src/engine/ecs/HierarchyComponents.h
git commit -m "Add DisabledTag, ActiveStateComponent, and hierarchy components"
```

---

### Task 2: Create GameRegistry wrapper

Header-only class wrapping `entt::registry`. Its `view()` auto-excludes `DisabledTag`; `viewAll()` returns unfiltered results. Forwards the ~11 registry methods used across the codebase.

**Files:**
- Create: `src/engine/ecs/GameRegistry.h`

**Step 1: Create GameRegistry.h**

```cpp
#pragma once

#include <entt/entt.hpp>
#include "engine/ecs/DisabledTag.h"

class GameRegistry {
public:
    // --- Filtered views (default for all game systems) ---

    template<typename... Components>
    auto view() {
        return registry_.view<Components...>(entt::exclude<DisabledTag>);
    }

    template<typename... Components>
    auto view() const {
        return registry_.view<Components...>(entt::exclude<DisabledTag>);
    }

    // --- Unfiltered views (editor, serialization, level loading) ---

    template<typename... Components>
    auto viewAll() {
        return registry_.view<Components...>();
    }

    template<typename... Components>
    auto viewAll() const {
        return registry_.view<Components...>();
    }

    // --- Entity lifecycle ---

    entt::entity create() { return registry_.create(); }
    void destroy(entt::entity e) { registry_.destroy(e); }
    bool valid(entt::entity e) const { return registry_.valid(e); }

    // --- Component access ---

    template<typename T, typename... Args>
    T& emplace(entt::entity e, Args&&... args) {
        return registry_.emplace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& emplace_or_replace(entt::entity e, Args&&... args) {
        return registry_.emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& get_or_emplace(entt::entity e, Args&&... args) {
        return registry_.get_or_emplace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T& get(entt::entity e) { return registry_.get<T>(e); }

    template<typename T>
    const T& get(entt::entity e) const { return registry_.get<T>(e); }

    template<typename T>
    T* try_get(entt::entity e) { return registry_.try_get<T>(e); }

    template<typename T>
    const T* try_get(entt::entity e) const { return registry_.try_get<T>(e); }

    template<typename... T>
    bool all_of(entt::entity e) const { return registry_.all_of<T...>(e); }

    template<typename... T>
    bool any_of(entt::entity e) const { return registry_.any_of<T...>(e); }

    template<typename T>
    void remove(entt::entity e) { registry_.remove<T>(e); }

    template<typename T>
    std::size_t remove(entt::entity e) requires (!std::is_void_v<T>) {
        return registry_.remove<T>(e);
    }

    // --- Context variables ---

    auto& ctx() { return registry_.ctx(); }
    const auto& ctx() const { return registry_.ctx(); }

    // --- Escape hatch for unwrapped EnTT API ---

    entt::registry& raw() { return registry_; }
    const entt::registry& raw() const { return registry_; }

private:
    entt::registry registry_;
};
```

**Step 2: Commit**

```bash
git add src/engine/ecs/GameRegistry.h
git commit -m "Add GameRegistry wrapper with auto-filtered views"
```

---

### Task 3: Test GameRegistry auto-filtering

Verify that `view()` excludes entities with `DisabledTag` and `viewAll()` includes them.

**Files:**
- Create: `tests/engine/test_game_registry.cpp`
- Modify: `tests/engine/CMakeLists.txt`

**Step 1: Write the test**

```cpp
#include "engine/ecs/GameRegistry.h"
#include "engine/ecs/DisabledTag.h"

#include <cassert>

struct Position { float x = 0.0f; };
struct Velocity { float v = 0.0f; };

int main() {
    // --- view() excludes DisabledTag ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1, Position{1.0f});
        reg.emplace<Position>(e2, Position{2.0f});
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto [entity, pos] : reg.view<Position>().each()) {
            assert(entity == e1);
            count++;
        }
        assert(count == 1);
    }

    // --- viewAll() includes DisabledTag ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1, Position{1.0f});
        reg.emplace<Position>(e2, Position{2.0f});
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto entity : reg.viewAll<Position>()) {
            (void)entity;
            count++;
        }
        assert(count == 2);
    }

    // --- view() with multiple components ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1);
        reg.emplace<Velocity>(e1);
        reg.emplace<Position>(e2);
        reg.emplace<Velocity>(e2);
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto entity : reg.view<Position, Velocity>()) {
            assert(entity == e1);
            count++;
        }
        assert(count == 1);
    }

    // --- emplace, get, try_get, valid, all_of, destroy ---
    {
        GameRegistry reg;
        auto e = reg.create();
        reg.emplace<Position>(e, Position{42.0f});

        assert(reg.valid(e));
        assert(reg.all_of<Position>(e));
        assert(reg.get<Position>(e).x == 42.0f);
        assert(reg.try_get<Position>(e) != nullptr);
        assert(reg.try_get<Velocity>(e) == nullptr);

        reg.destroy(e);
        assert(!reg.valid(e));
    }

    return 0;
}
```

**Step 2: Register the test in CMakeLists**

Find the existing `tests/engine/CMakeLists.txt` and add:

```cmake
pixel_roguelike_add_test(test_game_registry
    SOURCES test_game_registry.cpp
    LIBRARIES engine_core
    LABELS engine
)
```

**Step 3: Build and run**

```bash
cmake --build build --target test_game_registry && ctest --test-dir build -R game_registry -V
```

Expected: all assertions pass, exit code 0.

**Step 4: Commit**

```bash
git add tests/engine/test_game_registry.cpp tests/engine/CMakeLists.txt
git commit -m "Add tests for GameRegistry auto-filtered views"
```

---

### Task 4: Create EntityActiveState API

Functions for enable/disable with hierarchy propagation.

**Files:**
- Create: `src/engine/ecs/EntityActiveState.h`
- Create: `src/engine/ecs/EntityActiveState.cpp`
- Modify: `src/engine/CMakeLists.txt` — add EntityActiveState.cpp to engine_core

**Step 1: Create EntityActiveState.h**

```cpp
#pragma once

#include "engine/ecs/GameRegistry.h"

// Set an entity active or inactive, propagating through the hierarchy.
// When disabling: adds DisabledTag to entity and all descendants.
// When enabling: removes DisabledTag from entity and descendants,
//   unless a descendant has activeSelf == false (explicitly disabled).
void setEntityActive(GameRegistry& registry, entt::entity entity, bool active);

// Returns the entity's explicit active state (true if never explicitly disabled).
bool isActiveSelf(const GameRegistry& registry, entt::entity entity);

// Returns whether the entity is actually active in the hierarchy
// (no DisabledTag present).
bool isActiveInHierarchy(const GameRegistry& registry, entt::entity entity);
```

**Step 2: Create EntityActiveState.cpp**

```cpp
#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/ActiveStateComponent.h"
#include "engine/ecs/HierarchyComponents.h"

namespace {

void disableRecursive(GameRegistry& registry, entt::entity entity) {
    if (!registry.all_of<DisabledTag>(entity)) {
        registry.emplace<DisabledTag>(entity);
    }
    auto* children = registry.try_get<ChildrenComponent>(entity);
    if (children) {
        for (auto child : children->children) {
            disableRecursive(registry, child);
        }
    }
}

void enableRecursive(GameRegistry& registry, entt::entity entity) {
    // If this entity was explicitly disabled, stop — it stays disabled
    auto* state = registry.try_get<ActiveStateComponent>(entity);
    if (state && !state->activeSelf) {
        return;
    }

    registry.raw().remove<DisabledTag>(entity);

    auto* children = registry.try_get<ChildrenComponent>(entity);
    if (children) {
        for (auto child : children->children) {
            enableRecursive(registry, child);
        }
    }
}

bool hasDisabledAncestor(const GameRegistry& registry, entt::entity entity) {
    auto* parent = registry.try_get<ParentComponent>(entity);
    if (!parent || parent->parent == entt::null) {
        return false;
    }
    if (registry.all_of<DisabledTag>(parent->parent)) {
        return true;
    }
    return hasDisabledAncestor(registry, parent->parent);
}

} // namespace

void setEntityActive(GameRegistry& registry, entt::entity entity, bool active) {
    auto& state = registry.get_or_emplace<ActiveStateComponent>(entity);
    state.activeSelf = active;

    if (!active) {
        disableRecursive(registry, entity);
    } else {
        // Only actually enable if no ancestor is disabled
        if (!hasDisabledAncestor(registry, entity)) {
            enableRecursive(registry, entity);
        }
    }
}

bool isActiveSelf(const GameRegistry& registry, entt::entity entity) {
    auto* state = registry.try_get<ActiveStateComponent>(entity);
    return state == nullptr || state->activeSelf;
}

bool isActiveInHierarchy(const GameRegistry& registry, entt::entity entity) {
    return !registry.all_of<DisabledTag>(entity);
}
```

**Step 3: Add to engine_core CMakeLists**

In `src/engine/CMakeLists.txt`, add `ecs/EntityActiveState.cpp` to the `engine_core` source list:

```cmake
add_library(engine_core STATIC
    core/Window.cpp
    core/Application.cpp
    core/PathUtils.cpp
    core/ProjectConfig.cpp
    core/Time.cpp
    ecs/EntityActiveState.cpp
)
```

**Step 4: Commit**

```bash
git add src/engine/ecs/EntityActiveState.h src/engine/ecs/EntityActiveState.cpp src/engine/CMakeLists.txt
git commit -m "Add setEntityActive API with hierarchy propagation"
```

---

### Task 5: Test hierarchy enable/disable

Verify the full Unity-style propagation behavior.

**Files:**
- Create: `tests/engine/test_entity_active_state.cpp`
- Modify: `tests/engine/CMakeLists.txt`

**Step 1: Write the test**

```cpp
#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/HierarchyComponents.h"
#include "engine/ecs/ActiveStateComponent.h"

#include <cassert>

struct Renderable {};

// Helper: link parent → child in hierarchy components
void linkParentChild(GameRegistry& reg, entt::entity parent, entt::entity child) {
    reg.emplace<ParentComponent>(child, ParentComponent{parent});
    auto& children = reg.get_or_emplace<ChildrenComponent>(parent);
    children.children.push_back(child);
}

int main() {
    // --- Disable single entity (no children) ---
    {
        GameRegistry reg;
        auto e = reg.create();
        reg.emplace<Renderable>(e);

        assert(isActiveSelf(reg, e));
        assert(isActiveInHierarchy(reg, e));

        setEntityActive(reg, e, false);
        assert(!isActiveSelf(reg, e));
        assert(!isActiveInHierarchy(reg, e));

        // view() should exclude it
        int count = 0;
        for (auto entity : reg.view<Renderable>()) { (void)entity; count++; }
        assert(count == 0);

        setEntityActive(reg, e, true);
        assert(isActiveSelf(reg, e));
        assert(isActiveInHierarchy(reg, e));
    }

    // --- Disable parent propagates to children ---
    {
        GameRegistry reg;
        auto room = reg.create();
        auto table = reg.create();
        auto lamp = reg.create();
        auto chair = reg.create();

        reg.emplace<Renderable>(room);
        reg.emplace<Renderable>(table);
        reg.emplace<Renderable>(lamp);
        reg.emplace<Renderable>(chair);

        linkParentChild(reg, room, table);
        linkParentChild(reg, room, chair);
        linkParentChild(reg, table, lamp);

        setEntityActive(reg, room, false);

        assert(!isActiveInHierarchy(reg, room));
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveInHierarchy(reg, lamp));
        assert(!isActiveInHierarchy(reg, chair));

        // All still have activeSelf true except room
        assert(!isActiveSelf(reg, room));
        assert(isActiveSelf(reg, table));
        assert(isActiveSelf(reg, lamp));
        assert(isActiveSelf(reg, chair));
    }

    // --- Re-enable parent, but child with activeSelf=false stays disabled ---
    {
        GameRegistry reg;
        auto room = reg.create();
        auto table = reg.create();
        auto lamp = reg.create();
        auto chair = reg.create();

        reg.emplace<Renderable>(room);
        reg.emplace<Renderable>(table);
        reg.emplace<Renderable>(lamp);
        reg.emplace<Renderable>(chair);

        linkParentChild(reg, room, table);
        linkParentChild(reg, room, chair);
        linkParentChild(reg, table, lamp);

        // Explicitly disable table first
        setEntityActive(reg, table, false);
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveInHierarchy(reg, lamp));  // child of disabled table

        // Now disable room
        setEntityActive(reg, room, false);

        // Re-enable room
        setEntityActive(reg, room, true);

        // room and chair should be active
        assert(isActiveInHierarchy(reg, room));
        assert(isActiveInHierarchy(reg, chair));

        // table was explicitly disabled — stays disabled
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveSelf(reg, table));

        // lamp is child of disabled table — stays disabled
        assert(!isActiveInHierarchy(reg, lamp));
    }

    // --- Enable entity with disabled ancestor → stays disabled ---
    {
        GameRegistry reg;
        auto parent = reg.create();
        auto child = reg.create();
        linkParentChild(reg, parent, child);

        setEntityActive(reg, parent, false);
        setEntityActive(reg, child, false);

        // Try to re-enable child while parent is disabled
        setEntityActive(reg, child, true);
        assert(isActiveSelf(reg, child));          // activeSelf is true
        assert(!isActiveInHierarchy(reg, child));  // but parent is disabled
    }

    return 0;
}
```

**Step 2: Register the test**

Add to `tests/engine/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_entity_active_state
    SOURCES test_entity_active_state.cpp
    LIBRARIES engine_core
    LABELS engine
)
```

**Step 3: Build and run**

```bash
cmake --build build --target test_entity_active_state && ctest --test-dir build -R entity_active_state -V
```

Expected: all assertions pass, exit code 0.

**Step 4: Commit**

```bash
git add tests/engine/test_entity_active_state.cpp tests/engine/CMakeLists.txt
git commit -m "Add tests for hierarchy enable/disable propagation"
```

---

### Task 6: Migrate all entt::registry references to GameRegistry

Mechanical refactor: replace `entt::registry` with `GameRegistry` across every header and source file. Every function signature, member variable, and return type that uses `entt::registry&` changes to `GameRegistry&`.

This is a bulk find-and-replace. The compiler will catch anything missed.

**Files to modify (headers — each has a corresponding .cpp that also needs updating):**

**Engine layer:**
- `src/engine/core/Application.h:52,107` — member type + accessor return type
- `src/engine/physics/PhysicsSystem.h:23-24` — `init()`/`update()` parameter

**Game layer:**
- `src/game/runtime/RuntimeGameSession.h:70-71,91` — member type + accessor return types
- `src/game/runtime/RuntimeGameplay.h:18-40` — all 8 free function signatures
- `src/game/level/LevelBuildContext.h:9` — struct member
- `src/game/level/LevelBuilder.h:21` — `registry()` return type
- `src/game/rendering/RuntimeSceneRenderer.h:22,24,51-61` — ~6 method parameters
- `src/game/rendering/RuntimeCameraMath.h:24` — function parameter
- `src/game/rendering/EnvironmentDebugSync.h:37` — function parameter
- `src/game/systems/KinematicColliderSystem.h:7` — function parameter
- `src/game/systems/RenderSystem.h:28,30` — method parameters
- `src/game/systems/AudioListenerSystem.h:15` — function parameter
- `src/game/behavior/BehaviorSystem.h:19` — callback typedef parameter
- `src/game/modules/door/DoorAnimationSystem.h:14,18` — function parameters
- `src/game/modules/door/DoorActionHandler.h:11` — function parameter

**Editor layer:**
- `src/editor/scene/EditorPreviewWorld.h:40-41,59` — member type + accessor return types
- `src/editor/core/EditorRuntimePreviewSession.h:43-44` — accessor return types
- `src/editor/render/EditorScenePreviewRenderer.h:21` — function parameter

**Step 1: For each header file listed above:**

Replace every occurrence of `entt::registry&` with `GameRegistry&` and `entt::registry ` (member) with `GameRegistry `.

Add include `#include "engine/ecs/GameRegistry.h"` and remove `#include <entt/entt.hpp>` if GameRegistry.h is the only reason entt was included (keep it if the file also uses `entt::entity` or other EnTT types directly — GameRegistry.h already includes entt).

**Step 2: For each corresponding .cpp file:**

Apply the same type replacements. Add `#include "engine/ecs/GameRegistry.h"` if not already pulled in via the header.

**Step 3: Update editor files that should use viewAll()**

In these files, change `registry.view<>()` calls to `registry.viewAll<>()` since the editor needs to see disabled entities:

- `src/editor/render/EditorScenePreviewRenderer.cpp` — `collectLights()` uses `registry.view<TransformComponent, LightComponent>()`; change to `registry.viewAll<>()`

**Step 4: Build**

```bash
cmake --build build 2>&1 | head -50
```

Fix any remaining compile errors (likely missed files or include paths). The compiler will report exactly which files still reference `entt::registry` where `GameRegistry` is expected.

**Step 5: Run all tests**

```bash
ctest --test-dir build -V
```

All existing tests must still pass.

**Step 6: Commit**

```bash
git add -A
git commit -m "Migrate entt::registry to GameRegistry across codebase"
```

---

### Task 7: Build runtime hierarchy in LevelLoader

Add a second pass after entity spawning that constructs `ParentComponent` and `ChildrenComponent` from the existing `parentNodeId` relationships already in `LevelDef` placement data.

**Files:**
- Modify: `src/game/level/LevelLoader.cpp`

**Step 1: Add includes**

At the top of `LevelLoader.cpp`, add:

```cpp
#include "engine/ecs/HierarchyComponents.h"
```

**Step 2: Add hierarchy building pass**

After the existing NodeIndex building block (lines 142-150) and after the kinematic linking block (lines 152-166), add a new block that builds parent-child relationships for ALL entities. This needs access to the original LevelDef placements and the NodeIndex.

Insert after line 166 (after the kinematic linking block):

```cpp
    // Third pass: build runtime parent-child hierarchy from parentNodeId
    {
        const auto& nodeIndex = registry.ctx().get<NodeIndex>();

        auto linkParent = [&](const std::string& nodeId, const std::string& parentNodeId) {
            if (nodeId.empty() || parentNodeId.empty()) return;
            entt::entity child = nodeIndex.resolve(nodeId);
            entt::entity parent = nodeIndex.resolve(parentNodeId);
            if (child == entt::null || parent == entt::null) return;
            if (registry.all_of<ParentComponent>(child)) return; // already linked (e.g. kinematic)
            registry.emplace<ParentComponent>(child, ParentComponent{parent});
            auto& children = registry.get_or_emplace<ChildrenComponent>(parent);
            children.children.push_back(child);
        };

        for (const auto& m : level.meshes) linkParent(m.nodeId, m.parentNodeId);
        for (const auto& l : level.lights) linkParent(l.nodeId, l.parentNodeId);
        for (const auto& c : level.colliders) linkParent(c.nodeId, c.parentNodeId);
        for (const auto& r : level.reflectionProbes) linkParent(r.nodeId, r.parentNodeId);
        for (const auto& a : level.archetypes) linkParent(a.nodeId, a.parentNodeId);
    }
```

**Step 3: Build and verify**

```bash
cmake --build build && ctest --test-dir build -V
```

**Step 4: Commit**

```bash
git add src/game/level/LevelLoader.cpp
git commit -m "Build runtime parent-child hierarchy during level loading"
```

---

### Task 8: Integration test — hierarchy from scene file

Verify that loading a scene with parent-child relationships results in working hierarchy components and enable/disable propagation.

**Files:**
- Create: `tests/game/test_entity_hierarchy.cpp`
- Modify: `tests/game/CMakeLists.txt`

**Step 1: Write the test**

```cpp
#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/GameRegistry.h"
#include "engine/ecs/HierarchyComponents.h"
#include "game/behavior/NodeIdComponent.h"
#include "game/behavior/NodeIndex.h"
#include "game/components/MeshComponent.h"
#include "game/components/TransformComponent.h"

#include <cassert>

// Simulate what LevelLoader does: create entities with nodeIds and parentNodeIds,
// build NodeIndex, then build hierarchy.
int main() {
    GameRegistry reg;

    // Create a scene: room → table → lamp
    auto room = reg.create();
    reg.emplace<NodeIdComponent>(room, NodeIdComponent{"room_1"});
    reg.emplace<TransformComponent>(room);

    auto table = reg.create();
    reg.emplace<NodeIdComponent>(table, NodeIdComponent{"table_1"});
    reg.emplace<TransformComponent>(table);

    auto lamp = reg.create();
    reg.emplace<NodeIdComponent>(lamp, NodeIdComponent{"lamp_1"});
    reg.emplace<TransformComponent>(lamp);

    // Build NodeIndex
    NodeIndex nodeIndex;
    nodeIndex.add("room_1", room);
    nodeIndex.add("table_1", table);
    nodeIndex.add("lamp_1", lamp);

    // Simulate parentNodeId linking (room is parent of table, table is parent of lamp)
    auto linkParent = [&](entt::entity child, const std::string& parentNodeId) {
        entt::entity parent = nodeIndex.resolve(parentNodeId);
        if (parent != entt::null) {
            reg.emplace<ParentComponent>(child, ParentComponent{parent});
            auto& children = reg.get_or_emplace<ChildrenComponent>(parent);
            children.children.push_back(child);
        }
    };
    linkParent(table, "room_1");
    linkParent(lamp, "table_1");

    // Verify hierarchy was built
    assert(reg.all_of<ParentComponent>(table));
    assert(reg.get<ParentComponent>(table).parent == room);
    assert(reg.all_of<ChildrenComponent>(room));
    assert(reg.get<ChildrenComponent>(room).children.size() == 1);
    assert(reg.get<ChildrenComponent>(room).children[0] == table);

    // Disable room → table and lamp should also be disabled
    setEntityActive(reg, room, false);
    assert(!isActiveInHierarchy(reg, room));
    assert(!isActiveInHierarchy(reg, table));
    assert(!isActiveInHierarchy(reg, lamp));

    // view() should return nothing
    int count = 0;
    for (auto entity : reg.view<TransformComponent>()) { (void)entity; count++; }
    assert(count == 0);

    // Re-enable room → everything comes back
    setEntityActive(reg, room, true);
    assert(isActiveInHierarchy(reg, room));
    assert(isActiveInHierarchy(reg, table));
    assert(isActiveInHierarchy(reg, lamp));

    count = 0;
    for (auto entity : reg.view<TransformComponent>()) { (void)entity; count++; }
    assert(count == 3);

    return 0;
}
```

**Step 2: Register the test**

Add to `tests/game/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_entity_hierarchy
    SOURCES test_entity_hierarchy.cpp
    LIBRARIES engine_core gameplay
    LABELS game
)
```

**Step 3: Build and run**

```bash
cmake --build build --target test_entity_hierarchy && ctest --test-dir build -R entity_hierarchy -V
```

**Step 4: Commit**

```bash
git add tests/game/test_entity_hierarchy.cpp tests/game/CMakeLists.txt
git commit -m "Add integration test for entity hierarchy and enable/disable"
```

---

### Task 9: Full build verification

Build all three executables and run the full test suite to verify nothing is broken.

**Step 1: Full build**

```bash
cmake --build build 2>&1 | tail -20
```

Expected: build succeeds with no errors.

**Step 2: Run all tests**

```bash
ctest --test-dir build -V
```

Expected: all tests pass (including all pre-existing tests).

**Step 3: Launch level editor briefly to verify it starts**

```bash
./build/level-editor &
sleep 2
kill %1
```

Expected: editor launches without crash. (Manual verification — if it opens a window and renders, we're good.)
