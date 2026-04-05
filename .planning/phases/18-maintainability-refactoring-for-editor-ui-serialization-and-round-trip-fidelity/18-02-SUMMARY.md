---
phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
plan: 02
subsystem: serialization
tags: [scene-format, parser, serializer, lights, level-def, round-trip]

# Dependency graph
requires:
  - phase: 18-01
    provides: parseNodeMetadata helper and per-action parser functions in LevelDef.cpp

provides:
  - Unified light parser handling point/spot/directional via single 'light <type>' branch
  - Unified light serializer writing 'light point/spot/directional' format
  - All 6 scene files migrated to new unified light format (50 light lines)
  - Round-trip test covering all three light types

affects:
  - scene file loading
  - editor scene serialization
  - behavior sub-line routing for lights (currentKind tracking preserved)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Unified light parser: single 'light' keyword with subtype token (point/spot/directional), mirroring Phase 17 collider unification"
    - "Light serializer switch: switch on LightType, prefix each branch with 'light <type>'"

key-files:
  created: []
  modified:
    - src/game/level/LevelDef.cpp
    - tests/game/test_level_roundtrip.cpp
    - assets/scenes/cathedral.scene
    - assets/scenes/country_house.scene
    - assets/scenes/initial_scene.scene
    - assets/scenes/institutional_room.scene
    - assets/scenes/silos_cloister.scene
    - assets/scenes/warden_office.scene

key-decisions:
  - "Unified light format: 'light point/spot/directional' replaces separate light/spot_light/dir_light keywords, matching Phase 17 collider pattern"
  - "Shadow bool for spot lights remains positional (read directly from stream before collectRemainingTokens) to preserve backward-compatible field order"
  - "Serializer uses switch on LightType with a single appendNodeMetadata call after the switch block — avoids code duplication"

patterns-established:
  - "Entity type unification: read subtype token, branch per subtype for field parsing — established by colliders (Ph17), now applied to lights"

requirements-completed: []

# Metrics
duration: 15min
completed: 2026-04-05
---

# Phase 18 Plan 02: Light Keyword Unification Summary

**Unified light/spot_light/dir_light into a single 'light point/spot/directional' parser and serializer, migrating 50 light lines across 6 scene files**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-04-05T00:00:00Z
- **Completed:** 2026-04-05T00:15:00Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- Replaced three separate parser blocks (light/spot_light/dir_light) with a single unified block reading a subtype token
- Replaced three serializer branches with a switch statement writing 'light point/spot/directional'
- Migrated all 50 light lines across all 6 scene files — no old keywords remain
- Extended test_level_roundtrip.cpp with point and directional light test cases (previously only spot was tested)

## Task Commits

1. **Task 1: Unify light parser and serializer in LevelDef.cpp** - `edc4670` (feat)
2. **Task 2: Migrate all scene files to unified light format** - `56231d6` (feat)

## Files Created/Modified

- `src/game/level/LevelDef.cpp` - Unified parser (3 blocks -> 1) and serializer (3 branches -> switch)
- `tests/game/test_level_roundtrip.cpp` - Added point and directional light cases; assert new format present, old format absent
- `assets/scenes/cathedral.scene` - 10 light -> light point, 3 spot_light -> light spot
- `assets/scenes/country_house.scene` - 12 light -> light point
- `assets/scenes/initial_scene.scene` - 7 light -> light point
- `assets/scenes/institutional_room.scene` - 6 light -> light point
- `assets/scenes/silos_cloister.scene` - 1 dir_light -> light directional, 3 light -> light point
- `assets/scenes/warden_office.scene` - 8 light -> light point

## Decisions Made

- Used a switch on LightType for the serializer (rather than if/else chain) to match the clean pattern the collider serializer established
- Shadow bool for spot lights kept as a positional stream read before collectRemainingTokens — this preserves the exact field order from the old format
- `currentKind = CurrentEntityKind::Light` set before push (not after) per Pitfall 5 from RESEARCH.md, ensuring behavior sub-lines route correctly

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Light format is fully unified; parser only handles the new format
- Scene files are all on the new format and parse correctly
- Round-trip test covers point, spot, and directional lights
- Ready for Plan 03 (inspector decomposition) or Plan 04 (visitor pattern)

---
*Phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity*
*Completed: 2026-04-05*
