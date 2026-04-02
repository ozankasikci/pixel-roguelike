---
phase: quick
plan: 260402-uz2
subsystem: assets/scenes
tags: [materials, scene, textures, QuestDoorsPack]
key-files:
  created:
    - assets/materials/qdp_wall.material
    - assets/materials/qdp_floor.material
  modified:
    - assets/scenes/initial_scene.scene
decisions: []
metrics:
  duration: 2m
  completed: "2026-04-02T19:24:45Z"
---

# Quick Task 260402-uz2: Fix walls/floors to use QuestDoorsPack textured materials

Wire QuestDoorsPack PBR texture materials (qdp_wall, qdp_floor) to SM_Wall/SM_Floor meshes in initial_scene.scene, replacing flat procedural colors; set uv_mode mesh for baked FBX UVs.

## Task Summary

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Update scene material references and fix material UV modes | f7d1abe | initial_scene.scene, qdp_wall.material, qdp_floor.material |

## Changes Made

- Created `assets/materials/qdp_wall.material` with PBR texture maps (albedo, normal, roughness, AO) from QuestDoorsPack Wall textures, `uv_mode mesh`
- Created `assets/materials/qdp_floor.material` with PBR texture maps (albedo, normal, roughness) from QuestDoorsPack Floor textures, `uv_mode mesh`
- Updated all 20 SM_Floor lines in initial_scene.scene from `material inst_glossy_floor` to `material qdp_floor`
- Updated all 15 SM_Wall lines in initial_scene.scene from `material inst_beige_wall` to `material qdp_wall`
- Left 3 prison_wall_door lines unchanged on `inst_beige_wall` (procedural mesh with no baked UVs)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Created missing qdp_wall.material and qdp_floor.material**
- **Found during:** Task 1
- **Issue:** Plan stated previous task (260402-uc1) created these material files, but they were only created as untracked files in the main working directory and never committed. They did not exist in the worktree.
- **Fix:** Created both material files from scratch with correct PBR texture paths, `uv_mode mesh`, and `roughness_bias 0.5` (matching qdp_door_a.material reference)
- **Files created:** assets/materials/qdp_wall.material, assets/materials/qdp_floor.material
- **Commit:** f7d1abe

## Verification

- 0 remaining `inst_glossy_floor` references in scene
- 20 `qdp_floor` references (all SM_Floor lines)
- 15 `qdp_wall` references (all SM_Wall lines)
- 3 `prison_wall_door` lines still use `inst_beige_wall` (correct)
- Both material files use `uv_mode mesh`

## Known Stubs

None.
