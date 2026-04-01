---
phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens
plan: "02"
subsystem: rendering
tags: [cpp, debug-ui, imgui, refactoring, structs]

requires: []
provides:
  - CameraDebugInfo struct in engine/rendering for camera debug display data
  - RuntimeLightingOverride struct in game/rendering for lighting debug tuning
  - Slimmed DebugParams with nested camera and lighting sub-structs
affects:
  - Any plan that reads or writes DebugParams fields (camera, shadow, hemisphere, directional light)

tech-stack:
  added: []
  patterns:
    - "Sub-struct decomposition: large flat debug structs decomposed into focused sub-structs by domain layer (engine vs game)"
    - "Nested field paths: DebugParams.camera.* for camera info, DebugParams.lighting.* for lighting overrides"

key-files:
  created:
    - src/engine/rendering/CameraDebugInfo.h
    - src/game/rendering/RuntimeLightingOverride.h
  modified:
    - src/engine/ui/ImGuiLayer.h
    - src/engine/ui/ImGuiLayer.cpp
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/game/rendering/EnvironmentDebugSync.cpp

key-decisions:
  - "CameraDebugInfo lives in engine/rendering (camera info is engine-layer concern)"
  - "RuntimeLightingOverride lives in game/rendering (lighting override is game-layer concern)"
  - "DebugParams retains only UI overlay state (post, internalResIndex, resolutionChanged, fps, frameTimeMs, drawCalls) plus two embedded sub-structs"

patterns-established:
  - "Sub-struct by layer: engine-layer data in engine headers, game-layer data in game headers — avoids upward layer dependencies"

requirements-completed: []

duration: 15min
completed: 2026-04-01
---

# Phase 12 Plan 02: DebugParams Decomposition Summary

**DebugParams 40-field struct decomposed into CameraDebugInfo and RuntimeLightingOverride sub-structs, reducing recompilation blast radius and clarifying layer ownership across 6 consuming files**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-04-01T18:00:00Z
- **Completed:** 2026-04-01T18:13:21Z
- **Tasks:** 2
- **Files modified:** 6 (2 created, 4 modified)

## Accomplishments
- Created `CameraDebugInfo` in engine/rendering with position, direction, fov, moveSpeed fields
- Created `RuntimeLightingOverride` in game/rendering with all shadow/hemisphere/directional light fields and exact default values matching previous DebugParams
- Slimmed DebugParams from ~40 fields to 7 fields (post, camera, lighting, internalResIndex, resolutionChanged, fps, frameTimeMs, drawCalls)
- Updated all 6 field-accessing files to use nested paths (`params.camera.*`, `params.lighting.*`)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create CameraDebugInfo.h and RuntimeLightingOverride.h** - `1d1ce63` (feat)
2. **Task 2: Slim DebugParams and update all 9 consuming files** - `a9e1378` (refactor)

## Files Created/Modified
- `src/engine/rendering/CameraDebugInfo.h` - New struct: camera position/direction/fov/moveSpeed
- `src/game/rendering/RuntimeLightingOverride.h` - New struct: shadow/hemisphere/directional light fields
- `src/engine/ui/ImGuiLayer.h` - DebugParams slimmed; includes new sub-struct headers
- `src/engine/ui/ImGuiLayer.cpp` - Camera and Lighting overlay sections updated to use nested paths
- `src/game/rendering/RuntimeSceneRenderer.cpp` - updateDebugParams, collectLights, render all updated
- `src/game/rendering/EnvironmentDebugSync.cpp` - syncSkySunFromDirectional and applyEnvironmentSettings updated

## Decisions Made
- CameraDebugInfo placed in `engine/rendering/` — camera display data is engine-layer concern, not game-layer
- RuntimeLightingOverride placed in `game/rendering/` — lighting overrides are game-layer concern (shadow/hemisphere/directional light settings)
- RenderSystem.h, RuntimeGameSession.h, EditorRuntimePreviewSession.h needed no field-access changes — they only hold/pass DebugParams as a type

## Deviations from Plan

None - plan executed exactly as written.

The 9 files listed in the plan included 3 that only hold DebugParams as a member/parameter type (RenderSystem.h, RuntimeGameSession.h, EditorRuntimePreviewSession.h) — those required no field-access updates, just that the struct compiles, which it does.

## Issues Encountered

Build verification for `pixel-roguelike` target failed due to a pre-existing cross-agent conflict: another parallel agent (commit `ff21b51`) refactored `LevelLoader.h`, changing the `load()` method signature, which broke `RuntimeGameSession.cpp`. This is outside the scope of this plan.

Targeted build verification for `engine_ui` and `game_rendering` targets (the targets containing files I modified) completed successfully with zero errors.

## Known Stubs

None.

## Next Phase Readiness
- CameraDebugInfo and RuntimeLightingOverride are available for import by downstream plans
- Any plan writing camera state to debug params should use `params.camera.*` paths
- Any plan reading/writing lighting state should use `params.lighting.*` paths
- Inter-agent LevelLoader conflict should be resolved before full pixel-roguelike build can pass

---
*Phase: 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens*
*Completed: 2026-04-01*

## Self-Check: PASSED

- FOUND: src/engine/rendering/CameraDebugInfo.h
- FOUND: src/game/rendering/RuntimeLightingOverride.h
- FOUND commit: 1d1ce63
- FOUND commit: a9e1378
