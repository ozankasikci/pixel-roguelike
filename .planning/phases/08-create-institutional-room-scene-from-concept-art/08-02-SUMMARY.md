---
phase: 08-create-institutional-room-scene-from-concept-art
plan: 02
subsystem: assets
tags: [scene, materials, interaction, institutional, cpp]

requires:
  - 08-01 (institutional materials, environment profile, procedural meshes)
provides:
  - assets/scenes/institutional_room.scene (complete loadable institutional room scene)
  - GenericFileScene locked door interaction stubs (metal door + chained door with "This door is locked" prompt)
affects: [runtime-scene-loader, editor-scene-picker, interaction-system]

tech-stack:
  added: []
  patterns:
    - "buildScriptedGeometry conditional pattern: if levelId == scene_name, spawn scripted entities with components not expressible in .scene file format"
    - "Locked door stub: addMesh + emplace<InteractableComponent> with locked prompt — no DoorComponent, just interaction feedback"

key-files:
  created:
    - assets/scenes/institutional_room.scene
  modified:
    - src/game/scenes/GenericFileScene.cpp

key-decisions:
  - "Metal and chained doors come from buildScriptedGeometry (not .scene file) so InteractableComponent can be attached — .scene format has no component syntax"
  - "Open wooden door is static geometry in .scene file with rotation 0,-90,0 (swung open) and warm golden backlight at Z=6.5"
  - "Scene file uses prison_wall_door for all three door openings on north wall, with edge-filler panels at X=-3.5 and X=+3.5 using 0.5 X-scale"

patterns-established:
  - "Institutional room coord system: X -4 to +4, Z 0 to +6, Y 0 to +4, doors on north (Z=6) wall"
  - "Scene-specific scripted geometry: check levelId in GenericFileScene::onEnter(), branch buildScriptedGeometry lambda accordingly"

requirements-completed: []

duration: 2min
completed: 2026-04-01
---

# Phase 08 Plan 02: Institutional Room Scene File Summary

**Complete institutional_room.scene file with warm beige walls, glossy floor, crown molding, two ceiling light pairs, three doors (open wooden + locked metal + locked chained with padlock), HVAC vent, smoke detector, and locked door interaction stubs wired via buildScriptedGeometry**

## Performance

- **Duration:** ~2 min
- **Started:** 2026-03-31T21:39:15Z
- **Completed:** 2026-04-01
- **Tasks:** 1 of 2 (awaiting visual checkpoint for Task 2)
- **Files modified:** 3

## Accomplishments
- Complete `assets/scenes/institutional_room.scene` with 8m x 6m x 4m room geometry matching concept art
- 12 floor tiles (inst_glossy_floor), 12 ceiling tiles (ceiling_default warm tint), 10 beige wall panels, 3 door-wall panels on north wall
- 16 baseboards + 16 crown molding pieces using inst_dark_trim for institutional trim aesthetic
- 4 ceiling light panels with warm-white point lights (1.0, 0.97, 0.88) intensity 5.5 radius 4.5
- Open wooden door at X=-2.5 (static, swung open), warm golden backlight at Z=6.5 intensity 8.0
- HVAC vent mesh above center door, smoke detector with red LED point light on ceiling center, chain/padlock on right door
- 6 box colliders for floor, ceiling, south wall, north wall, east wall, west wall
- Player spawn at Z=1.5 facing north (+Z direction, yaw 0)
- GenericFileScene.cpp modified to conditionally set buildScriptedGeometry for institutional_room:
  - Metal door (center) via addMesh + InteractableComponent "E  This door is locked"
  - Chained door (right) via addMesh + InteractableComponent "E  This door is locked"

## Task Commits

1. **Task 1: Create institutional_room.scene and wire locked door interaction stubs** - `86de000` (feat)

## Files Created/Modified
- `assets/scenes/institutional_room.scene` - Complete institutional room scene file per D-01 through D-16
- `src/game/scenes/GenericFileScene.cpp` - Added InteractableComponent includes and buildScriptedGeometry conditional for institutional_room

## Decisions Made
- Metal and chained doors come from `buildScriptedGeometry` lambda (not .scene file) because the .scene format does not support `InteractableComponent` attachment — this is the clean path per the plan's Pitfall 6 note
- Open wooden door uses rotation `0, -90, 0` to swing it open (away from player view into the room beyond)
- Edge filler panels at X=-3.5 and X=+3.5 use 0.5 X-scale to cover the 1m gaps at the north wall extremes

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

The following interaction stubs exist by design for Plan 02 scope — they are intentional:

- **Metal door interaction** (`src/game/scenes/GenericFileScene.cpp:38`): Shows "E  This door is locked" but has no open/close behavior — future phase will add DoorComponent + trigger
- **Chained door interaction** (`src/game/scenes/GenericFileScene.cpp:52`): Same stub — future phase will add DoorComponent + trigger
- **Wooden door**: Static geometry only, no interaction component — by design (D-03), future phase may animate this

These stubs satisfy the Phase 8 goal (interaction feedback visible to player); functional open/close is out of scope for this phase.

## Next Phase Readiness
- institutional_room.scene loads via GenericFileScene with correct environment profile, materials, and meshes
- Build verified passing (exit 0)
- Visual verification pending (Task 2 checkpoint awaiting human approval)

---
*Phase: 08-create-institutional-room-scene-from-concept-art*
*Completed: 2026-04-01*

## Self-Check: PASSED

- FOUND: assets/scenes/institutional_room.scene
- FOUND: src/game/scenes/GenericFileScene.cpp (modified with InteractableComponent)
- FOUND commit: 86de000 (Task 1)
- BUILD: pixel-roguelike exit code 0
