---
phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline
plan: 01
subsystem: rendering
tags: [materials, content-pipeline, scene-files, feature-flags, roughness, specular]

requires:
  - phase: 06-data-driven-scene-management
    provides: Scene file format with material <id> syntax support in LevelDef parser

provides:
  - MaterialDefinition structs with feature flags (animated, subsurface, detailBrick, detailWood, detailStone, detailFloor) and specularLevel
  - ContentRegistry auto-discovers .material files via recursive_directory_iterator with duplicate detection and inheritance validation
  - All 13 .material files with roughness_bias, specular_level, and feature flags baked from shader defaults
  - All 3 .scene files using explicit material <id> syntax (no bare MaterialKind tokens)

affects:
  - 07-02 (MaterialKind enum removal — depends on feature flags being in material data model)
  - 07-03 (shader integration — depends on specular_level and feature flags being readable from resolved materials)

tech-stack:
  added: []
  patterns:
    - "ContentRegistry uses recursive_directory_iterator for auto-discovery instead of hardcoded file lists"
    - "Material inheritance: parent has base roughness/specular, child overrides only if explicitly set"
    - "Scene files use material <id> syntax exclusively; legacy bare MaterialKind tokens are fully migrated"

key-files:
  created: []
  modified:
    - src/game/rendering/MaterialDefinition.h
    - src/game/rendering/MaterialDefinition.cpp
    - src/game/content/ContentRegistry.h
    - src/game/content/ContentRegistry.cpp
    - assets/materials/masonry_base.material
    - assets/materials/stone_default.material
    - assets/materials/brick_default.material
    - assets/materials/wood_default.material
    - assets/materials/metal_default.material
    - assets/materials/wax_default.material
    - assets/materials/moss_default.material
    - assets/materials/viewmodel_default.material
    - assets/materials/floor_default.material
    - assets/materials/ceiling_default.material
    - assets/materials/cloister_stone.material
    - assets/materials/concrete_wall.material
    - assets/scenes/cathedral.scene
    - tests/game/test_material_definitions.cpp
    - tests/game/test_level_def.cpp

key-decisions:
  - "roughness_bias in .material files now bakes the per-kind shader default (stone 0.82, wood 0.74, metal 0.34, wax 0.58, moss 0.94, viewmodel 0.48, floor 0.86, brick 0.88)"
  - "specular_level default of 0.20f in ResolvedMaterialDefinition matches stone default — root materials without parent must set explicit specular_level or inherit stone default"
  - "ContentRegistry::loadMaterialsFromDirectory uses recursive_directory_iterator — any .material file placed in assets/materials/ or a subdirectory is auto-loaded"
  - "detail_stone false explicitly set on brick_default to override masonry_base's detail_stone true — child overrides work correctly via optional<bool>"
  - "cloister_stone keeps roughness_bias 0.82 (stone value) even though it previously had 0.06, since the parent masonry_base now provides the correct base"

requirements-completed:
  - MAT-DATA-MODEL
  - MAT-AUTO-SCAN
  - MAT-SCENE-MIGRATE

duration: 35min
completed: 2026-03-30
---

# Phase 07 Plan 01: Material Data Model Foundation Summary

**Feature flags (animated, subsurface, detailBrick, detailWood, detailStone, detailFloor) and specularLevel added to MaterialDefinition; ContentRegistry replaces hardcoded file list with recursive auto-scanner; all 13 .material files bake shader roughness/specular defaults; cathedral.scene fully migrated to material ID syntax**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-03-30T19:20:00Z
- **Completed:** 2026-03-30T19:55:00Z
- **Tasks:** 2
- **Files modified:** 19

## Accomplishments

- Added 7 new optional fields to MaterialDefinition and 7 resolved fields to ResolvedMaterialDefinition, with full parse/resolve/serialize support
- Replaced hardcoded std::array materialFiles in ContentRegistry::loadDefaults() with recursive_directory_iterator scan, duplicate detection via error log, and inheritance validation
- Baked per-kind shader defaults into all 13 .material files: roughness_bias, specular_level, and appropriate detail/animated/subsurface feature flags
- Migrated cathedral.scene (104 mesh lines) from bare MaterialKind tokens to `material <id>` syntax; silos_cloister.scene and warden_office.scene were already using explicit syntax
- Added feature flag parsing/inheritance/roundtrip tests to test_material_definitions.cpp

## Task Commits

1. **Task 1: MaterialDefinition feature flags, ContentRegistry auto-scanner** - `e0e9bd0` (feat)
2. **Task 2: Bake shader defaults into .material files, migrate cathedral.scene, update tests** - `5fedcf6` (feat)

**Plan metadata:** (created in this commit)

## Files Created/Modified

- `src/game/rendering/MaterialDefinition.h` - Added specularLevel, animated, subsurface, detailBrick, detailWood, detailStone, detailFloor to both MaterialDefinition and ResolvedMaterialDefinition
- `src/game/rendering/MaterialDefinition.cpp` - Parser, resolver, serializer support for all new fields
- `src/game/content/ContentRegistry.h` - Added loadMaterialsFromDirectory() and validateMaterialInheritance() declarations
- `src/game/content/ContentRegistry.cpp` - Recursive auto-scanner replacing hardcoded array, spdlog for warnings/errors
- `assets/materials/*.material` (all 13) - roughness_bias, specular_level, feature flags baked from shader defaults
- `assets/scenes/cathedral.scene` - 104 mesh lines migrated from bare tokens to `material <id>` syntax
- `tests/game/test_material_definitions.cpp` - Feature flag parsing, inheritance, override, and roundtrip tests added
- `tests/game/test_level_def.cpp` - Updated assertions to use materialId field instead of legacy material enum field

## Decisions Made

- Roughness values baked directly into each material's roughness_bias field using materialRoughness() shader defaults per kind — no runtime lookup needed
- cloister_stone roughness_bias updated from 0.06 to 0.82 (stone default) — old value was too low given masonry_base is now the correct stone baseline
- detail_stone false explicitly set on brick_default to override masonry_base inheritance — demonstrates the optional<bool> override pattern works correctly

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] test_level_def brick count assertion used legacy material field**
- **Found during:** Task 2 (cathedral.scene migration)
- **Issue:** test_level_def.cpp line 19-21 counted bricks via `mesh.material.has_value() && *mesh.material == MaterialKind::Brick` — this legacy field is only set by bare token parsing. After migration to `material <id>` syntax, the count was 0.
- **Fix:** Updated test to use `mesh.materialId == "brick_default"` and corrected count from 15 to 16 (one mesh was already using `material brick_default` syntax in the original file, so was always missed by the legacy field check).
- **Files modified:** tests/game/test_level_def.cpp
- **Verification:** test_level_def passes
- **Committed in:** 5fedcf6 (Task 2 commit)

**2. [Rule 1 - Bug] test_level_def wax/wood assertions used legacy material field**
- **Found during:** Task 2 (cathedral.scene migration)
- **Issue:** Same pattern — wax/wood counts used `mesh.material.has_value()` which is only populated by bare token parsing
- **Fix:** Updated to use materialId field (`mesh.materialId == "wax_default"`, `mesh.materialId == "wood_default"`)
- **Files modified:** tests/game/test_level_def.cpp
- **Verification:** test_level_def passes
- **Committed in:** 5fedcf6 (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2 Rule 1 — bugs in test assertions exposed by scene migration)
**Impact on plan:** Both fixes necessary for test suite to reflect the migrated scene format. No scope creep.

## Issues Encountered

- **test_content_registry pre-existing failure:** `registry.findEnvironment("neutral") != nullptr` fails because `neutral.environment`, `cloister_daylight.environment`, and `game_ready_neutral.environment` files do not exist on disk. This failure predates this plan (confirmed by testing HEAD before any changes). Documented in `deferred-items.md`. The three passing tests (test_material_definitions, test_level_def, test_level_roundtrip) cover all changes introduced by this plan.

## Known Stubs

None — all material values are real shader defaults baked from the GLSL source. All scene file migrations use actual material IDs that exist on disk.

## Next Phase Readiness

- MaterialDefinition data model now has all feature flags needed for Plan 02 (MaterialKind enum removal)
- ContentRegistry auto-scanner is in place; new .material files placed in assets/materials/ are automatically loaded
- Plan 02 can safely remove the `shading_model` / `MaterialKind` field from both structs and replace with feature flags
- Scene files are fully migrated; Plan 02 does not need to touch scene files for material syntax

---
*Phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline*
*Completed: 2026-03-30*
