# Phase 21: Feature Module Reorganization — Folder-per-feature with door module as proof-of-concept - Context

**Gathered:** 2026-04-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Reorganize the codebase from type-based organization (all components in `components/`, all systems in `systems/`) to feature-based modules. Create `src/game/modules/door/` as the proof-of-concept module that owns all door-related code — components, system, spawner, serializer, inspector, and action types. Establish the module infrastructure (CMake target, parser registration, action handler registration) that future modules will follow.

</domain>

<decisions>
## Implementation Decisions

### Module boundary and layer policy
- **D-01:** Co-locate ALL code for a feature in one directory, including editor-only code (inspectors). Organizational clarity takes precedence over the strict engine→game→editor layer boundary. `src/game/modules/door/` contains game logic, animation system, spawner, serializer, AND DoorGroupInspector.
- **D-02:** Editor-only files (e.g., `DoorInspector.cpp`) are conditionally added via CMake — only compiled when building the editor target. No `#ifdef` preprocessor macros in source code. The CMake source list for the module separates core vs editor files.

### CMake target structure
- **D-03:** Each module is its own CMake static library. The door module becomes `game_module_door`. Modules declare their dependencies explicitly (e.g., `game_module_door` depends on `engine_core`, `engine_rendering`, `engine_physics`, `gameplay`). This gives proper isolation and explicit dependency graphs.
- **D-04:** The `editor` target links `game_module_door` with the editor source files included. The `pixel-roguelike` target links `game_module_door` without editor source files. The `gameplay` target does NOT include module code — modules are separate libraries that depend on `gameplay`.

### Serialization delegation
- **D-05:** Modules register keyword parser callbacks at startup. When LevelDef's parser encounters a keyword, it looks up the registered handler and delegates. This decouples the monolithic parser from specific features — LevelDef.cpp no longer needs to `#include` door headers.
- **D-06:** Registration happens during module initialization (a `registerDoorModule()` function called from app startup). Parser maintains a `std::unordered_map<std::string, ParseCallback>` for keyword→handler lookup.
- **D-07:** Serialization (writing .scene files) also uses registered callbacks — each module provides both a parser and a serializer for its keywords. Full round-trip ownership within the module.

### Action type ownership
- **D-08:** Door-specific action types (OpenDoor, CloseDoor, ToggleDoor) and DoorActionParams move out of the central ActionTypes.h into the door module. The central ActionTypes.h retains only shared/base types.
- **D-09:** BehaviorSystem dispatches to registered action handlers, similar to parser registration. Door module registers its action handler at startup. BehaviorSystem no longer needs to know about door-specific action types.

### Migration scope
- **D-10:** Files that move into `src/game/modules/door/`:
  - DoorConfigComponent.h, DoorStateComponent.h, DoorLeafComponent.h (from `components/`)
  - DoorAnimationSystem.h/.cpp (from `behavior/`)
  - Door spawning logic from LevelBuilder.cpp (extracted into DoorSpawner.cpp)
  - Door serialization logic from LevelDef.cpp (extracted into DoorSerializer.cpp)
  - DoorGroupInspector.h/.cpp (from `editor/ui/inspectors/`)
  - makePivotLeafModel() from GameplayPrefabs (into door module as DoorMath or similar)
  - DoorActionParams and door action types from ActionTypes.h
- **D-11:** Door-related test files move to a corresponding `tests/game/modules/door/` directory.

### Claude's Discretion
- Internal file naming within the module directory (e.g., `DoorMath.h` vs `DoorPivot.h` for pivot math)
- Exact registration API design (function signatures, init ordering)
- How to extract door spawning logic from LevelBuilder.cpp cleanly (may need a spawner interface)
- Whether the parser registration map lives in LevelDef or a separate ParserRegistry class
- How to handle the `door_group` keyword migration (keep vs rename to `door`)
- Whether a `modules/` README or template is needed for documenting the module pattern

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Door system (current state after Phase 19)
- `src/game/components/DoorConfigComponent.h` — Door config component (spawn-time, immutable)
- `src/game/components/DoorStateComponent.h` — Door runtime state (progress, target state)
- `src/game/components/DoorLeafComponent.h` — Per-leaf animation data
- `src/game/behavior/DoorAnimationSystem.h` — Single door animation system
- `src/game/behavior/DoorAnimationSystem.cpp` — Animation tick, leaf model updates
- `src/game/behavior/ActionTypes.h` — DoorActionParams, OpenDoor/CloseDoor/ToggleDoor
- `src/game/prefabs/GameplayPrefabs.h` — makePivotLeafModel() pivot math
- `src/game/prefabs/GameplayPrefabs.cpp` — Pivot math implementation
- `src/game/level/LevelBuilder.cpp` — Door spawning via addDoorGroup()
- `src/game/level/LevelDef.h` — LevelDoorPlacement struct
- `src/game/level/LevelDef.cpp` — door_group parser and serializer
- `src/editor/ui/inspectors/DoorGroupInspector.h` — Editor door inspector
- `src/editor/ui/inspectors/DoorGroupInspector.cpp` — Editor door inspector impl

### CMake structure
- `CMakeLists.txt` — Root CMake config
- `src/game/CMakeLists.txt` — Game layer targets (game_content, game_rendering, gameplay)
- `src/editor/CMakeLists.txt` — Editor target

### Architecture reference
- `.planning/codebase/ARCHITECTURE.md` — Layer design, system execution phases
- `.planning/codebase/STRUCTURE.md` — Current directory layout
- `.planning/codebase/CONVENTIONS.md` — Naming, code style, include organization
- `.planning/notes/module-pattern-decision.md` — Module pattern decision record

### Prior refactoring context
- Phase 18 CONTEXT.md — Inspector decomposition pattern, parser consolidation, visitor pattern
- Phase 19 CONTEXT.md — Door system unification, DoorConfigComponent/DoorStateComponent split

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- Phase 18 already decomposed EditorInspectorPanel into per-type inspector files — DoorGroupInspector is standalone and ready to move
- Phase 19 unified door system to single DoorAnimationSystem — no duplicate implementations to reconcile
- LevelDef.cpp parser was consolidated in Phase 18 with parseNodeMetadata() helpers — extraction points are clean

### Established Patterns
- Service locator pattern (`Application::emplaceService<T>()`) — could be used for parser/action registries
- Phase-ordered system execution (Input → Interaction → Physics → Gameplay → Camera → Render) — door system registered at Gameplay phase
- CMake FetchContent for dependencies, static libraries per engine subsystem

### Integration Points
- `Application::init()` — where module registration would be called
- `LevelDef::loadFromFile()` — where parser callback dispatch would be injected
- `BehaviorSystem::executeAction()` — where action handler dispatch would be injected
- `RuntimeGameSession` — where module systems get ticked
- `LevelEditorCore` — where editor-side module components get initialized

</code_context>

<specifics>
## Specific Ideas

- The user emphasized: "You open the folder, you see the whole feature. You delete the folder, the feature is gone."
- Module is JUST a folder with its own CMake target — no abstract `GameplayModule` interface, no runtime reflection, no dynamic loading
- Door is the proof-of-concept. If the pattern works well, checkpoints, cameras, inventory follow later in separate phases.

</specifics>

<deferred>
## Deferred Ideas

- Migrating checkpoints, cameras, inventory, and other interactive objects to the module pattern — separate future phases after door module proves the pattern
- Whether to create a `ModuleTemplate/` directory or documentation for creating new modules
- Potential for a module generator script/tool

</deferred>

---

*Phase: 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept*
*Context gathered: 2026-04-07*
