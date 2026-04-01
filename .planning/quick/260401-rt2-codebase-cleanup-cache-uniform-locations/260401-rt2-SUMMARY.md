---
phase: quick
plan: 260401-rt2
subsystem: engine/rendering, engine/physics, game/rendering
tags: [performance, cleanup, deduplication]
dependency_graph:
  requires: []
  provides: [cached-uniform-locations, jolt-body-guard, shared-math-utils, renderer-scratch-vectors]
  affects: [Shader, PhysicsSystem, RuntimeSceneRenderer, MathUtils]
tech_stack:
  added: []
  patterns: [uniform-location-cache, scratch-vector-reuse, shared-math-headers]
key_files:
  created: []
  modified:
    - src/engine/rendering/core/Shader.h
    - src/engine/rendering/core/Shader.cpp
    - src/engine/physics/PhysicsSystem.cpp
    - src/engine/core/MathUtils.h
    - src/game/rendering/RuntimeSceneRenderer.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/game/level/LevelDef.cpp
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - src/editor/render/EditorScenePreviewRenderer.cpp
    - src/game/levels/GameAssets.cpp
decisions:
  - Shader uniform cache uses mutable unordered_map with uniformLocation() private method; set* methods remain const
  - makeModelMatrix in MathUtils uses early-exit rotation guards (if != 0) unlike the removed local copies which always called glm::rotate
  - RuntimeSceneRenderer scratch vectors are mutable member fields to keep collect* methods const-qualified
  - collectLights uses a local reference alias (lights) so the body reads identically to the original
metrics:
  duration: ~15 minutes
  completed: 2026-04-01
  tasks_completed: 2
  files_modified: 11
---

# Phase quick Plan 260401-rt2: Codebase Cleanup — Cache Uniform Locations Summary

One-liner: Per-frame glGetUniformLocation cost eliminated via a mutable unordered_map cache on Shader; Jolt invalid body ID silently dropped is now logged and skipped; extractRotationMatrix and makeModelMatrix deduplicated from five TUs into MathUtils.h; RuntimeSceneRenderer collect* vectors promoted to member fields to avoid per-frame heap allocation.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Cache shader uniform locations + guard Jolt body ID | 71ccb1c | Shader.h, Shader.cpp, PhysicsSystem.cpp |
| 2 | Extract shared utils to MathUtils.h + reuse renderer vectors | 639ae43 | MathUtils.h, LevelDef.cpp, EditorSceneDocument.cpp, EditorViewportInteraction.cpp, EditorScenePreviewRenderer.cpp, GameAssets.cpp, RuntimeSceneRenderer.h, RuntimeSceneRenderer.cpp |

## Changes Made

### Task 1: Cache Shader Uniform Locations

**Shader.h** — added `mutable std::unordered_map<std::string, GLint> uniform_cache_` and private method declaration `GLint uniformLocation(const std::string& name) const`.

**Shader.cpp** — implemented `uniformLocation()`: checks cache first, calls `glGetUniformLocation` on miss and stores the result. All five `set*` methods (`setMat4`, `setVec3`, `setVec2`, `setFloat`, `setInt`) now call `uniformLocation(name)` instead of a bare `glGetUniformLocation`.

**PhysicsSystem.cpp** — after `CreateAndAddBody` call at the static collider registration loop: added `if (bodyId.IsInvalid())` guard that logs a spdlog warning with entity ID and calls `continue` to skip `emplace` and the subsequent `find`.

### Task 2: Extract Shared Utils + Reuse Renderer Vectors

**MathUtils.h** — added two inline free functions after `makeTransformMatrix`:
- `extractRotationMatrix(const glm::mat4&)` — column-wise normalization to strip translation/scale
- `makeModelMatrix(position, scale, rotation)` — translate/rotate/scale with zero-rotation early-exit guards

**Removed local definitions from:**
- `src/game/level/LevelDef.cpp` — `extractRotationMatrix` (lines ~96-105)
- `src/editor/scene/EditorSceneDocument.cpp` — `extractRotationMatrix` in anonymous namespace
- `src/editor/viewport/EditorViewportInteraction.cpp` — `makeModelMatrix` in anonymous namespace
- `src/editor/render/EditorScenePreviewRenderer.cpp` — `makeModelMatrix` in anonymous namespace
- `src/game/levels/GameAssets.cpp` — `makeModel` in anonymous namespace + `} // namespace` closing brace; all ~28 call sites renamed from `makeModel(` to `makeModelMatrix(`

**RuntimeSceneRenderer** — changed `collectSceneObjects`, `collectViewmodelObjects`, `collectLights` from returning `std::vector` by value to accepting `std::vector<T>& out` params. Added three mutable member vectors (`scene_objects_`, `viewmodel_objects_`, `lights_`). `render()` now calls the void variants and passes member vectors directly to `SceneRenderInput`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Orphaned `} // namespace` in GameAssets.cpp**
- **Found during:** Task 2 build
- **Issue:** The original `makeModel` function and all cathedral helper functions were inside an anonymous `namespace { }` block. Removing `makeModel` and the namespace opener left the closing `} // namespace` at the bottom of the file as an orphaned brace, causing a compile error.
- **Fix:** Removed the orphaned `} // namespace` line (the cathedral helper functions do not require namespace scoping).
- **Files modified:** src/game/levels/GameAssets.cpp
- **Commit:** 639ae43 (same commit, fixed before final commit)

## Known Stubs

None. All changes are mechanical refactors with no placeholder data.

## Self-Check: PASSED
