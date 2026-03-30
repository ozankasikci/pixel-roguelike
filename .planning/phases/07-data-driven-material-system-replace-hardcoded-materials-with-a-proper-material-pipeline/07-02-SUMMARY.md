---
phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline
plan: 02
subsystem: rendering/materials
tags: [MaterialKind, enum-deletion, feature-flags, uber-shader, engine-game-layering]
requires: ["07-01"]
provides: ["07-03", "07-04"]
affects: [Renderer, MaterialTextureLibrary, MaterialDefinition, scene.frag, scene.vert, MeshComponent, LevelDef, LevelBuilder]
tech-stack:
  added: []
  patterns:
    - Feature flag uniforms replace MaterialKind enum branches in GLSL and C++
    - MaterialTextureLibrary returns magenta fallback RenderMaterialData for unknown IDs
    - Engine layer (Renderer.h) no longer imports game-layer types (MaterialKind.h)
key-files:
  deleted:
    - src/game/rendering/MaterialKind.h
  modified:
    - src/engine/rendering/geometry/Renderer.h
    - src/engine/rendering/geometry/Renderer.cpp
    - src/game/rendering/MaterialDefinition.h
    - src/game/rendering/MaterialDefinition.cpp
    - src/game/rendering/MaterialTextureLibrary.h
    - src/game/rendering/MaterialTextureLibrary.cpp
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/game/level/LevelBuilder.h
    - src/game/level/LevelBuilder.cpp
    - src/game/components/MeshComponent.h
    - src/game/prefabs/GameplayPrefabs.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/core/EditorRuntimePreviewSession.cpp
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/ui/EditorPanelUtils.cpp
    - src/editor/ui/LevelEditorUi.h
    - apps/model_viewer/main.cpp
    - tests/game/test_material_definitions.cpp
    - tests/game/test_level_roundtrip.cpp
    - tests/game/test_content_registry.cpp
    - tests/editor/test_editor_hierarchy.cpp
    - assets/shaders/game/scene.frag
    - assets/shaders/game/scene.vert
decisions:
  - "MaterialKind enum deleted; feature flags (detailBrick, detailStone, etc.) are authoritative source on ResolvedMaterialDefinition"
  - "shading_model key in .material files silently ignored for backward compat — root material validation no longer requires it"
  - "MaterialTextureLibrary.definitionFor() returns const pointer (nullptr if not found) instead of throwing"
  - "Magenta fallback (1.0, 0.0, 1.0) returned by resolve() for unknown materialId — visible in renderer to aid debugging"
  - "Roughness formula simplified: clamp(uMaterialRoughnessScale * uMaterialRoughnessBias) — roughness_bias bakes the base roughness value"
  - "Engine Renderer.h no longer includes MaterialKind.h — engine/game layering violation fixed"
metrics:
  duration_minutes: 5
  completed_date: "2026-03-30"
  tasks_completed: 3
  tasks_planned: 3
  files_changed: 26
---

# Phase 07 Plan 02: Delete MaterialKind Enum and Refactor to Feature Flags Summary

**One-liner:** MaterialKind enum fully deleted — 30+ call sites migrated to property-driven feature flag uniforms (uMaterialAnimated, uMaterialBrickDetail, uMaterialStoneDetail, uMaterialWoodDetail, uMaterialFloorDetail, uMaterialSubsurface, uMaterialSpecularLevel) across engine, game, editor, shaders, and tests.

## Tasks Completed

| # | Task | Commit | Key Files |
|---|------|--------|-----------|
| 1+2 | Remove MaterialKind from all layers | a1d5d2b | MaterialKind.h deleted, Renderer.h, MaterialDefinition, MaterialTextureLibrary, LevelDef, LevelBuilder, MeshComponent, GameplayPrefabs, all editor files, apps/model_viewer, all tests |
| 3 | Refactor scene.frag + scene.vert to feature flag uniforms | 7cb518e | assets/shaders/game/scene.frag, assets/shaders/game/scene.vert |

## What Was Built

The MaterialKind enum (`Stone=0, Wood=1, Metal=2, Wax=3, Moss=4, Viewmodel=5, Floor=6, Brick=7`) has been completely eliminated from the codebase. Every layer was updated:

**Engine layer (Renderer.h / Renderer.cpp):**
- `RenderMaterialData.shadingModel: MaterialKind` replaced with 6 bool feature flags + `specularLevel: float`
- Renderer binds 7 individual uniforms instead of 1 `uMaterialKind` int
- Illegal engine→game include removed (`#include "game/rendering/MaterialKind.h"`)

**Game layer:**
- `MaterialDefinition.shadingModel` field removed; `shading_model` key in .material files silently ignored
- `MaterialTextureLibrary::resolve(id, kind)` → `resolve(id)` only; `definitionFor()` returns nullable pointer
- Magenta `(1,0,1)` fallback for unknown material IDs
- `LevelMeshPlacement.material: MaterialKind` field removed
- `MeshComponent.material: MaterialKind` field removed
- `LevelBuilder` default mesh material logic replaced with `defaultMaterialIdForMesh()` returning string IDs
- `GameplayPrefabs.cpp` updated from `MaterialKind::Wood` to `"wood_default"`

**Editor layer:**
- All `resolveHelperMaterial()` / `resolvePlacementMaterialKind()` helpers removed
- Inspector panel Shading Model combo box UI removed (was dead UI for MaterialKind)
- `syncMaterials()` simplified to string-only materialId comparison

**Shaders:**
- `scene.vert`: `uMaterialKind` removed, flame vertex deformation uses `uMaterialAnimated != 0`
- `scene.frag`: Complete rewrite — 8 `MATERIAL_*` constants removed, `materialRoughness()` / `materialMetalness()` / `materialSpecularLevel()` / `materialLightTintResponse()` helper functions deleted, all replaced with direct feature flag checks

**Apps + Tests:**
- `apps/model_viewer/main.cpp`: 24 presets converted from `MaterialKind::X` to string IDs
- All 5 test files updated to match new API (no MaterialKind in assertions or struct initializers)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] RuntimeSceneRenderer.cpp used removed material field**
- **Found during:** Task 1 build
- **Issue:** `RuntimeSceneRenderer.cpp:110` — `mesh.material` (MaterialKind field) used in `resolve()` call and cache key generation
- **Fix:** Updated `resolve(mesh.materialId, mesh.material)` → `resolve(mesh.materialId)`, replaced MaterialKind-based cache key with `"stone_default"` string
- **Files modified:** `src/game/rendering/RuntimeSceneRenderer.cpp`
- **Commit:** a1d5d2b (included in Task 1+2 commit)

**2. [Rule 3 - Blocking] LevelLoader.cpp used removed material field**
- **Found during:** Task 1 build
- **Issue:** `LevelLoader.cpp:57` — `placement.material` passed to `addMesh()` whose signature no longer has that parameter
- **Fix:** Removed `placement.material` argument from `addMesh()` call
- **Files modified:** `src/game/level/LevelLoader.cpp`
- **Commit:** a1d5d2b

**3. [Rule 3 - Blocking] EditorScenePreviewRenderer.cpp used removed material field**
- **Found during:** Task 2 build
- **Issue:** `EditorScenePreviewRenderer.cpp:138` — `materials.resolve(mesh.materialId, mesh.material)` with old two-parameter signature
- **Fix:** Updated to `materials.resolve(mesh.materialId)` (one parameter)
- **Files modified:** `src/editor/render/EditorScenePreviewRenderer.cpp`
- **Commit:** a1d5d2b

**4. [Rule 2 - Missing critical functionality] Engine/game layering violation**
- **Found during:** Task 1 review
- **Issue:** `Renderer.h` in the engine layer included `game/rendering/MaterialKind.h` (game layer), violating the strict Engine → Game → Editor dependency direction
- **Fix:** Removed the illegal include along with the enum-based field it supported
- **Files modified:** `src/engine/rendering/geometry/Renderer.h`
- **Commit:** a1d5d2b

## Known Stubs

None. All feature flags are wired from `ResolvedMaterialDefinition` through `MaterialTextureLibrary::resolve()` to `RenderMaterialData` and bound as shader uniforms in `Renderer.cpp`.

## Self-Check: PASSED

Files confirmed present:
- assets/shaders/game/scene.frag: YES (uMaterialAnimated confirmed)
- assets/shaders/game/scene.vert: YES (uMaterialAnimated confirmed)
- src/engine/rendering/geometry/Renderer.h: YES (feature flags confirmed)
- src/game/rendering/MaterialKind.h: DELETED (confirmed)

Commits confirmed:
- a1d5d2b: YES
- 7cb518e: YES

Build: 100% — all targets built successfully, zero errors.
Shader grep: CLEAN — zero occurrences of uMaterialKind or MATERIAL_* in scene.frag/scene.vert.
MaterialKind grep: CLEAN — zero occurrences in src/, tests/, apps/, assets/.
