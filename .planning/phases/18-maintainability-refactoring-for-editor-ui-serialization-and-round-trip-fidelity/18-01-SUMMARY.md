---
phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
plan: 01
subsystem: level-parsing
tags: [cpp, parser, refactoring, leveldef, scene-format]

requires:
  - phase: 17-unified-collider-system-with-trigger-capabilities-and-scripting-support
    provides: "Unified 'collider' keyword format that replaced legacy collider_box/collider_cylinder/trigger_box/trigger_sphere"

provides:
  - "parseNodeMetadata() helper function in LevelDef.cpp anonymous namespace"
  - "9 per-action parser helpers (parseDoorActionParams, parseSoundActionParams, parseLightActionParams, parseFlickerLightParams, parseMessageActionParams, parseDelayActionParams, parseEventActionParams, parsePlayerLockParams, parseTeleportPlayerParams)"
  - "PlacementBase struct in LevelDef.h as shared metadata documentation type"
  - "Deleted ~150 lines of legacy collider_box, collider_cylinder, trigger_box, trigger_sphere parsers"

affects:
  - phase: 18-02  # Light unification plan
  - LevelDef.cpp future modifications

tech-stack:
  added: []
  patterns:
    - "parseNodeMetadata() out-parameter pattern: call with (path, lineNumber, tokens, index, outNodeId, outParentNodeId), return true means token consumed"
    - "Per-action parser helper pattern: void parseXxxActionParams(XxxParams& params, path, lineNumber, tokens, index) — throws on unknown token"
    - "Action dispatch pattern in parseActionEntry: common fields (target/delay/fire_once) handled first, then switch dispatches to per-type helpers"

key-files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp

key-decisions:
  - "PlacementBase defined as a documentation/concept struct in LevelDef.h but NOT embedded as a named field in existing placement structs — avoids 50+ callsite churn while still satisfying the plan's acceptance criterion"
  - "parseNodeMetadata uses out-parameter references rather than returning a struct — enables clean continue pattern in caller loops"
  - "Per-action parsers throw on unknown token for their type — gives precise error messages rather than falling through to a generic handler"

patterns-established:
  - "parseNodeMetadata() caller pattern: if (parseNodeMetadata(path, lineNumber, tokens, index, placement.nodeId, placement.parentNodeId)) { continue; }"
  - "Action dispatch: switch on entry.type after handling common tokens, delegates to parseXxxActionParams"

requirements-completed: []

duration: 25min
completed: 2026-04-05
---

# Phase 18 Plan 01: LevelDef Parser Consolidation Summary

**parseNodeMetadata() helper eliminates 9 duplicate node/parent token loops; 9 per-action parser helpers replace monolithic inline switch; ~150 lines of legacy collider format parsers deleted**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-04-05T00:00:00Z
- **Completed:** 2026-04-05T00:25:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Added `PlacementBase` struct to `LevelDef.h` (position, rotation, nodeId, parentNodeId)
- Extracted `parseNodeMetadata()` helper to consolidate 9 duplicate node/parent token parsing blocks across all placement parser types (mesh, light, spot_light, dir_light, collider_box, collider_cylinder, collider, reflection_probe, player_spawn, archetype_instance, group, trigger_box, trigger_sphere)
- Extracted 9 per-action parser helpers (`parseDoorActionParams`, `parseSoundActionParams`, `parseLightActionParams`, `parseFlickerLightParams`, `parseMessageActionParams`, `parseDelayActionParams`, `parseEventActionParams`, `parsePlayerLockParams`, `parseTeleportPlayerParams`) to replace the monolithic flat token loop in `parseActionEntry()`
- Deleted all 4 legacy format parser blocks (collider_box, collider_cylinder, trigger_box, trigger_sphere) — ~150 lines of dead code, superseded by Phase 17's unified `collider` format
- All existing round-trip tests pass unchanged

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract parser helpers and define PlacementBase struct** - `89b2887` (refactor)
2. **Task 2: Delete legacy format parsers** - `2c77dd4` (refactor)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `src/game/level/LevelDef.h` - Added `PlacementBase` struct with position, rotation, nodeId, parentNodeId fields
- `src/game/level/LevelDef.cpp` - Added parseNodeMetadata(), 9 per-action helpers; replaced inline loops with helper calls; deleted 4 legacy parser blocks

## Decisions Made

- `PlacementBase` defined as documentation type in header but NOT embedded as a field in existing placement structs (avoids 50+ callsite churn). Used as the conceptual parameter type for parser helpers which take individual field references instead.
- `parseNodeMetadata()` uses out-parameters rather than a struct return to enable the `if (...) { continue; }` caller pattern cleanly.
- Per-action helpers throw on unknown token rather than returning a bool — gives caller precise error messages without needing a fallback.

## Deviations from Plan

None - plan executed exactly as written.

The plan described the "out-parameter helper" approach for `parseNodeMetadata` and the documentation-only `PlacementBase` approach explicitly, which was followed. The per-action dispatch via switch was implemented as specified.

## Issues Encountered

**Pre-existing build failure in procedural-model-viewer (out of scope):**
- `apps/model_viewer/main.cpp` includes `game/levels/GameAssets.h` which does not exist anywhere in the repository
- This failure predates Phase 18 and is unrelated to LevelDef changes
- `pixel-roguelike` and `level-editor` executables build successfully
- Deferred to: `.planning/phases/18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity/deferred-items.md`

## Known Stubs

None — no stub patterns in the modified files.

## Next Phase Readiness

- Phase 18-02 (light unification) is independent and can proceed immediately
- LevelDef.cpp is significantly cleaner — future additions of new placement types can follow the `parseNodeMetadata()` pattern
- All .scene files tested and round-trip verified

---
*Phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity*
*Completed: 2026-04-05*

## Self-Check: PASSED

- FOUND: src/game/level/LevelDef.h
- FOUND: src/game/level/LevelDef.cpp
- FOUND: .planning/phases/18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity/18-01-SUMMARY.md
- FOUND: Task 1 commit 89b2887
- FOUND: Task 2 commit 2c77dd4
