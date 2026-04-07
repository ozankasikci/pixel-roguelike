---
title: Module pattern = folder-per-feature
date: 2026-04-07
context: Exploration session about project modularity and AI-friendly architecture
---

# Module Pattern Decision

**Decision:** Organize gameplay features by feature, not by type. A "module" is a directory that owns all code for a feature — components, system, spawner, serializer, editor inspector.

**Not:** A plugin framework with abstract interfaces, runtime registration, or `entt::meta` reflection. No `GameplayModule` base class. No dynamic loading.

**Structure example:**
```
src/game/modules/door/
  DoorComponents.h      // all door-related components
  DoorSystem.cpp         // animation + state machine (single implementation)
  DoorSpawner.cpp        // spawning logic (single place)
  DoorSerializer.cpp     // scene read/write
  DoorInspector.cpp      // editor UI
```

**Why:** The door system was spread across 34 files and 7 architectural layers. The same pivot math was duplicated in 3 places (LevelBuilder, EditorPreviewWorld, DoorAnimationSystem). The state machine had 2 implementations (BehaviorSystem, RuntimeGameplay). Any change to one required finding and updating all others.

**Principle:** You open the folder, you see the whole feature. You delete the folder, the feature is gone. Adding a new interactive object (lever, drawer) means copying the pattern into a new folder.

**Scope:** Start with doors as the proof-of-concept migration. Other systems (checkpoints, cameras, inventory) stay as-is until the pattern proves itself.
