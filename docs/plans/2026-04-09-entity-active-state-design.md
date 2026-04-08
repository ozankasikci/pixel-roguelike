# Entity Active State System

Date: 2026-04-09

## Overview

Unity-style enable/disable system for game entities. Disabling an entity removes it from all system processing (rendering, physics, gameplay). Supports hierarchical propagation — disabling a parent automatically disables all descendants.

## Core Concepts

Two levels of active state, mirroring Unity:

- **activeSelf** — whether this entity was explicitly enabled/disabled
- **activeInHierarchy** — whether this entity is actually active (self and all ancestors active)

An entity is only processed by systems when `activeInHierarchy` is true.

## Components

### DisabledTag

Zero-size marker component. When present, systems skip this entity. This is the runtime equivalent of `activeInHierarchy == false`.

```cpp
struct DisabledTag {};
```

### ActiveStateComponent

Tracks whether the entity was explicitly disabled (vs inherited from parent).

```cpp
struct ActiveStateComponent {
    bool activeSelf = true;
};
```

Only added to entities that have been explicitly disabled at least once. Entities without this component are implicitly `activeSelf = true`.

### ParentComponent

Runtime parent reference, built during level loading from existing `parentNodeId` data.

```cpp
struct ParentComponent {
    entt::entity parent = entt::null;
};
```

### ChildrenComponent

Runtime children list, built during level loading alongside ParentComponent.

```cpp
struct ChildrenComponent {
    std::vector<entt::entity> children;
};
```

## GameRegistry Wrapper

Replaces direct `entt::registry` usage. Default `view()` automatically excludes disabled entities.

```cpp
class GameRegistry {
public:
    // Auto-excludes DisabledTag — used by all game systems
    template<typename... Components>
    auto view();

    // Returns ALL entities including disabled — used by editor, serialization
    template<typename... Components>
    auto viewAll();

    // Forwards: create, destroy, emplace, try_get, get, remove, etc.

    // Escape hatch for unwrapped EnTT API
    entt::registry& raw();

private:
    entt::registry registry_;
};
```

Existing `registry.view<A, B>()` calls remain syntactically identical but now auto-filter disabled entities. Editor and serialization code uses `viewAll<>()`.

## Enable/Disable API

```cpp
void setEntityActive(GameRegistry& registry, entt::entity entity, bool active);
bool isActiveSelf(GameRegistry& registry, entt::entity entity);
bool isActiveInHierarchy(GameRegistry& registry, entt::entity entity);
```

### Disable (setEntityActive false)

1. Set `ActiveStateComponent.activeSelf = false`
2. Add `DisabledTag` to entity
3. Recursively walk descendants via `ChildrenComponent`
4. Add `DisabledTag` to each descendant

### Enable (setEntityActive true)

1. Set `ActiveStateComponent.activeSelf = true`
2. Check ancestors — if any ancestor has `DisabledTag`, stop (entity stays inactive in hierarchy)
3. Remove `DisabledTag` from entity
4. Recursively walk descendants — remove `DisabledTag` unless child has `activeSelf = false`

### Example

```
Room (active)
  ├── Table (explicitly disabled)     activeSelf=false, DisabledTag
  │     └── Lamp                      activeSelf=true,  DisabledTag (parent disabled)
  └── Chair (active)                  activeSelf=true,  no DisabledTag

Disable Room → all four entities get DisabledTag
Re-enable Room → Room and Chair lose DisabledTag
                 Table keeps DisabledTag (activeSelf=false)
                 Lamp keeps DisabledTag (parent Table still disabled)
```

## Integration

### Level Loading

After all entities are spawned and NodeIndex is built, a second pass constructs the runtime hierarchy:

```cpp
for (auto [entity, nodeId] : registry.viewAll<NodeIdComponent>()) {
    auto parentEntity = nodeIndex.resolve(parentNodeId);
    if (parentEntity != entt::null) {
        registry.emplace<ParentComponent>(entity, parentEntity);
        auto& children = registry.get_or_emplace<ChildrenComponent>(parentEntity);
        children.children.push_back(entity);
    }
}
```

### Scene Files

No format changes. Hierarchy is already defined via `parentNodeId`. Optionally, an `active: false` property on placements can make entities start disabled.

### Editor (future)

- Outliner uses `viewAll<>()` to show all entities, grays out disabled ones
- Inspector checkbox toggles `setEntityActive()`

### Physics (follow-up if needed)

PhysicsSystem queries are auto-filtered. If Jolt bodies need explicit removal/re-addition on disable/enable, that's a follow-up task.
