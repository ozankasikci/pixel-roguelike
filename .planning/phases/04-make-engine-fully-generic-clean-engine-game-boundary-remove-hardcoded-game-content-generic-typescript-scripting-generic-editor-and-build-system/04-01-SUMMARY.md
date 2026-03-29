---
phase: 04-make-engine-fully-generic
plan: 01
subsystem: engine-rendering
tags: [cpp, opengl, ecs, entt, material-system, engine-boundary]

# Dependency graph
requires: []
provides:
  - engine_rendering compiles with zero game-layer includes
  - RenderMaterialData uses int shadingModelIndex instead of MaterialKind enum
  - MeshComponent has only materialId string (no MaterialKind field)
  - MaterialTextureLibrary.resolve() takes only materialId (opaque int at engine boundary)
affects: [04-02, 04-03, game_rendering, gameplay, editor]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Engine/game boundary: engine types use opaque int for game-layer enum values"
    - "Game layer casts MaterialKind to int at the game/engine boundary before passing to engine"

key-files:
  created: []
  modified:
    - src/engine/rendering/geometry/Renderer.h
    - src/engine/rendering/geometry/Renderer.cpp
    - src/game/rendering/MaterialTextureLibrary.h
    - src/game/rendering/MaterialTextureLibrary.cpp
    - src/game/components/MeshComponent.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/editor/core/EditorRuntimePreviewSession.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp

key-decisions:
  - "Engine layer uses int shadingModelIndex=0 as default (0=Stone), not MaterialKind enum"
  - "MaterialTextureLibrary.resolve() takes only materialId string; legacyKind parameter removed"
  - "When materialId is empty the game layer resolves to stone_default (not MaterialKind fallback)"
  - "MeshComponent.material (MaterialKind) removed; materialId string is the sole material identifier"

patterns-established:
  - "Opaque int pattern: engine structs hold int shadingModelIndex; game layer casts enum to int before passing"
  - "Material fallback chain: materialId lookup -> stone_default (no legacyKind path in engine)"

requirements-completed:
  - ENG-BOUNDARY-01
  - ENG-BOUNDARY-02

# Metrics
duration: 25min
completed: 2026-03-29
---

# Phase 04 Plan 01: Decouple MaterialKind from Engine Rendering Layer Summary

**RenderMaterialData now uses `int shadingModelIndex` instead of `MaterialKind`, removing all game-layer includes from engine_rendering and eliminating the dual-field anti-pattern from MeshComponent**

## Performance

- **Duration:** 25 min
- **Started:** 2026-03-29T16:30:00Z
- **Completed:** 2026-03-29T16:53:13Z
- **Tasks:** 2
- **Files modified:** 13

## Accomplishments

- Removed `#include "game/rendering/MaterialKind.h"` from `Renderer.h`; engine_rendering now compiles with zero game-layer includes
- Replaced `MaterialKind shadingModel` field in `RenderMaterialData` with `int shadingModelIndex = 0`; game layer assigns `static_cast<int>(resolved.shadingModel)` at the boundary
- Removed `MaterialKind material` field from `MeshComponent` and `resolve(materialId, legacyKind)` second parameter from `MaterialTextureLibrary`; `materialId` string is now the sole material identifier throughout the stack

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace MaterialKind with int in engine headers and update game-layer resolve** - `f60955a`
2. **Task 2: Remove MaterialKind field from MeshComponent and update all callers** - `096ec8b`

**Plan metadata:** (to be committed with docs)

## Files Created/Modified

- `src/engine/rendering/geometry/Renderer.h` - Removed MaterialKind include; `shadingModel` field replaced with `int shadingModelIndex = 0`
- `src/engine/rendering/geometry/Renderer.cpp` - Pass `shadingModelIndex` directly to `uMaterialKind` uniform
- `src/game/rendering/MaterialTextureLibrary.h` - `resolve()` and `definitionFor()` now take only `materialId`
- `src/game/rendering/MaterialTextureLibrary.cpp` - Cast `resolved.shadingModel` to int at boundary; simplified `definitionFor()` fallback to `stone_default` directly
- `src/game/components/MeshComponent.h` - Removed `MaterialKind material` field and `MaterialKind.h` include
- `src/game/rendering/RuntimeSceneRenderer.cpp` - Updated three `resolve()` call sites; key computation uses `"stone_default"` fallback
- `src/game/level/LevelBuilder.cpp` - Fixed `MeshComponent` aggregate initialization (removed extra `MaterialKind` field)
- `src/game/level/LevelLoader.cpp` - Fixed `MeshComponent` aggregate initialization in `spawnViewmodelMesh`
- `src/editor/render/EditorScenePreviewRenderer.cpp` - Updated `resolveHelperMaterial` and `collectRenderObjects`
- `src/editor/core/EditorRuntimePreviewSession.cpp` - Removed `mesh.material` assignment in `syncMaterials`
- `src/editor/scene/EditorPreviewWorld.cpp` - Removed `mesh.material = resolvedMaterial` in `syncMaterials`
- `src/editor/ui/EditorInspectorPanel.cpp` - Updated `resolve()` call and two `shadingModel` references to `shadingModelIndex`
- `src/editor/viewport/EditorViewportInteraction.cpp` - Updated `resolveHelperMaterial` and placement resolve call

## Decisions Made

- `shadingModelIndex` defaults to `0` (Stone) — same integer value as the old `MaterialKind::Stone` default, preserving fallback behavior
- `definitionFor()` fallback path simplified: removed the `legacyKind` path that called `defaultMaterialIdForKind(legacyKind)`; now falls back to `stone_default` directly. This is safe because all callers that previously relied on `legacyKind` now pre-compute the materialId before calling `resolve()`

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed MeshComponent aggregate initialization in LevelBuilder.cpp**
- **Found during:** Task 2 (build verification)
- **Issue:** LevelBuilder was aggregate-initializing MeshComponent with 7 fields including `MaterialKind material`; after removing that field the struct has 6 fields and the old value became a type mismatch
- **Fix:** Removed the `resolvedMaterial` (MaterialKind) field from the initializer; `materialId.value_or(...)` remains as the 6th field
- **Files modified:** `src/game/level/LevelBuilder.cpp`
- **Verification:** `gameplay` target builds successfully
- **Committed in:** `096ec8b` (Task 2 commit)

**2. [Rule 1 - Bug] Fixed MeshComponent aggregate initialization in LevelLoader.cpp**
- **Found during:** Task 2 (build verification)
- **Issue:** `spawnViewmodelMesh` in LevelLoader initialized MeshComponent with 7 fields including `MaterialKind material`
- **Fix:** Removed the `material` (MaterialKind) field from the initializer
- **Files modified:** `src/game/level/LevelLoader.cpp`
- **Verification:** `gameplay` target builds successfully
- **Committed in:** `096ec8b` (Task 2 commit)

**3. [Rule 1 - Bug] Fixed EditorInspectorPanel.cpp resolve() call and shadingModel references**
- **Found during:** Task 2 (build verification)
- **Issue:** EditorInspectorPanel called `resolve(materialId, previewKind)` (2-arg form) and assigned `material.shadingModel` (removed field)
- **Fix:** Updated resolve call to 1-arg form; changed `shadingModel = *draft.shadingModel` to `shadingModelIndex = static_cast<int>(*draft.shadingModel)` and `shadingModel = MaterialKind::Stone` to `shadingModelIndex = 0`
- **Files modified:** `src/editor/ui/EditorInspectorPanel.cpp`
- **Verification:** `editor` target builds successfully
- **Committed in:** `096ec8b` (Task 2 commit)

**4. [Rule 1 - Bug] Fixed EditorViewportInteraction.cpp resolve() calls**
- **Found during:** Task 2 (build verification)
- **Issue:** Two `materials.resolve(materialId, kind)` calls in EditorViewportInteraction
- **Fix:** Updated both to 1-arg form with pre-computed materialId
- **Files modified:** `src/editor/viewport/EditorViewportInteraction.cpp`
- **Verification:** `editor` target builds successfully
- **Committed in:** `096ec8b` (Task 2 commit)

---

**Total deviations:** 4 auto-fixed (all Rule 1 - bugs found during build verification)
**Impact on plan:** All auto-fixes were cascade callers of the `MeshComponent` and `resolve()` API changes. No scope creep.

## Issues Encountered

Pre-existing build errors in `src/engine/input/InputSystem.cpp` and `apps/runtime/main.cpp` (unrelated to this plan — part of in-progress InputSystem refactoring on this branch). These do not affect the plan targets (`engine_rendering`, `game_rendering`, `gameplay`, `editor`), all of which build successfully.

## Next Phase Readiness

- `engine_rendering` compiles with zero game-layer includes (D-01 satisfied)
- `MeshComponent` has `materialId` string only (D-02 satisfied)
- Ready for Plan 04-02 which continues the engine/game boundary cleanup

---
*Phase: 04-make-engine-fully-generic*
*Completed: 2026-03-29*
