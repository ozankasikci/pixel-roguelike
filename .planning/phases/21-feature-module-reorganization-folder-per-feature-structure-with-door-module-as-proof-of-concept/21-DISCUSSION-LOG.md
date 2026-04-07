# Phase 21: Feature Module Reorganization - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-07
**Phase:** 21-feature-module-reorganization-folder-per-feature-structure-with-door-module-as-proof-of-concept
**Areas discussed:** Layer boundary vs module boundary, CMake target structure, Serialization delegation, Migration scope

---

## Layer boundary vs module boundary

| Option | Description | Selected |
|--------|-------------|----------|
| Split module | src/game/modules/door/ for game logic + src/editor/modules/door/ for inspector. Layer boundary preserved, two folders per module. | |
| Game-only module | src/game/modules/door/ for game logic only. Inspectors stay in src/editor/ui/inspectors/. Module pattern only applies to game layer. | |
| Co-locate everything | src/game/modules/door/ has ALL door code including inspector. Accept the layer boundary violation for organizational clarity. | ✓ |

**User's choice:** Co-locate everything
**Notes:** User prioritizes organizational clarity over strict layering.

### Follow-up: Editor-only code handling

| Option | Description | Selected |
|--------|-------------|----------|
| Compile guard | #ifdef EDITOR_BUILD around inspector code | |
| Separate CMake source list | CMake conditionally adds DoorInspector.cpp only for editor target | ✓ |
| Just compile it | Inspector compiles in both targets, linker strips unused | |

**User's choice:** Separate CMake source list
**Notes:** No preprocessor macros in code — CMake handles the conditional inclusion.

---

## CMake target structure

| Option | Description | Selected |
|--------|-------------|----------|
| Own static library | game_module_door as a separate CMake target. Explicit dependency graph. | ✓ |
| Directory within gameplay | Door module files stay part of gameplay target. Just folder reorganization. | |
| You decide | Claude's discretion based on existing CMake structure. | |

**User's choice:** Own static library
**Notes:** Each module as its own CMake static library for proper isolation.

---

## Serialization delegation

| Option | Description | Selected |
|--------|-------------|----------|
| Registration callbacks | Each module registers parse function for its keywords at startup. Parser dispatches dynamically. | ✓ |
| Direct includes | LevelDef.cpp includes DoorSerializer.h directly. Simple but coupled. | |
| Keep centralized | Serialization stays in LevelDef.cpp. Module only owns components, system, spawner, inspector. | |

**User's choice:** Registration callbacks
**Notes:** Fully decouples the parser from specific modules.

---

## Migration scope

| Option | Description | Selected |
|--------|-------------|----------|
| Move to module | DoorActionParams and door action types move to the door module. ActionTypes.h only has base types. | ✓ |
| Keep in ActionTypes.h | Action types are shared vocabulary. Door actions stay central. | |
| You decide | Claude's discretion based on registration callback approach. | |

**User's choice:** Move to module
**Notes:** Full ownership — door module owns its action types. BehaviorSystem dispatches to registered action handlers.

---

## Claude's Discretion

- Internal file naming within module directory
- Registration API design (function signatures, init ordering)
- How to extract door spawning logic from LevelBuilder.cpp
- Parser registration map location (LevelDef vs separate ParserRegistry)
- door_group keyword migration strategy

## Deferred Ideas

- Migrating other interactive objects (checkpoints, cameras, inventory) to module pattern — future phases
- Module template/documentation creation
- Module generator script/tool
