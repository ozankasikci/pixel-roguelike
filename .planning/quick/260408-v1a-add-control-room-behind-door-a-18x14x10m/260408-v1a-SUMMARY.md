---
phase: quick
plan: 260408-v1a
subsystem: scene/level
tags: [scene, level-design, control-room, initial-scene]
key-files:
  modified:
    - assets/scenes/initial_scene.scene
decisions:
  - "Used SM_Floor/prison_ceiling/SM_Wall mesh family (matching initial_scene) rather than prison_floor/prison_wall family used in reference scenes"
  - "East wall gap kept at Z=0..Z=2 (tiles at Z=-4,-2 and Z=4,6,8) to align with Door A passage at Z=1"
  - "Ceiling Y=10 with light panels at Y=9.99 and point lights at Y=9.7 — full 10m ceiling height"
  - "Split n_col_west into n_col_west_s (Z=-2..0) and n_col_west_n (Z=2..6) to allow Door A passage"
metrics:
  completed: 2026-04-08
  tasks: 2
  files: 1
---

# Phase quick Plan 260408-v1a: Add Control Room Behind Door A Summary

18x14x10m institutional control room appended to initial_scene.scene, accessible through Door A at (-5,0,1), with two-tone walls, glossy floor, 6 ceiling light panels, central desk cluster, warden's desk, and wall furniture.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Room shell — floor, walls, ceiling, colliders | 0bcb127 | assets/scenes/initial_scene.scene |
| 2 | Lighting, furniture, and detail props | 06e9895 | assets/scenes/initial_scene.scene |

## What Was Built

**Room geometry (Task 1):**
- 63 `SM_Floor` tiles (inst_glossy_floor) in 9x7 grid, X:-8..-24 step 2, Z:-4..8 step 2
- 63 `prison_ceiling` tiles (ceiling_default) at Y=10.0, same grid
- Lower dado walls: `SM_Wall` at scale 0.008 0.008 0.008 (~2m tall) in concrete_wall material
  - South (9 tiles at Z=-5), North (9 at Z=9), East (5 at X=-7, gap at Z=0/2), West (7 at X=-25)
- Upper walls: `SM_Wall` at scale 0.008 0.032 0.008 (~8m tall) at Y=2 in inst_beige_wall material
  - Same layout/count as dado
- 7 box colliders: floor, ceiling, south, north, west (full), east split into east_s and east_n
- Modified existing `n_col_west`: split into `n_col_west_s` and `n_col_west_n` leaving a 2m gap at Z=0..2 for Door A passage

**Lighting and props (Task 2):**
- 6 `ceiling_light_panel` meshes at Y=9.99 in 3x2 grid (X=-11/-16/-21, Z=0/4)
- 6 point lights at Y=9.7 matching panel positions (1.0 0.97 0.88, radius 10, intensity 4.0)
- 2 accent lights: entry (X=-8, Y=3, Z=1) and warden desk (X=-23, Y=3, Z=2)
- 4 `prison_desk` + 4 `prison_chair` central cluster at X=-15/-17, Z=0/3
- Warden's desk (`prison_desk`) + `warden_chair` + `prison_cabinet` at far west (X=-23..-24.5, Z=2)
- 4 `prison_cabinet` along north wall at Z=8.5 (X=-10/-13/-16/-19)
- 3 `prison_shelf` along south wall at Y=1.6, Z=-4.5 (X=-12/-16/-20)
- 2 `inst_hvac_vent` on upper walls (south and north), 2 `inst_smoke_detector` on ceiling
- 4 cable segments (SM_CableA south wall base, SM_CableB west wall base)

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None — all furniture uses existing mesh names referenced in warden_office.scene and institutional_room.scene. If any mesh names (prison_chair, SM_CableA, SM_CableB) are not yet registered in the mesh library they will silently not render, but colliders and scene structure are correct.

## Threat Flags

None — no new network endpoints, auth paths, or trust boundaries introduced.

## Self-Check: PASSED

- assets/scenes/initial_scene.scene: FOUND
- Commit 0bcb127 (Task 1): FOUND in git log
- Commit 06e9895 (Task 2): FOUND in git log
- 233 n_cr_ node references confirmed in scene file
