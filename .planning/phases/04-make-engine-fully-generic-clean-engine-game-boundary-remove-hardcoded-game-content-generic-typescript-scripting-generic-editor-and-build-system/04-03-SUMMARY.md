---
phase: 04-make-engine-fully-generic
plan: 03
subsystem: engine-game-boundary
tags: [entt, ecs, components, imgui, rendering, torch, overlays]

# Dependency graph
requires:
  - phase: 04-01
    provides: MeshComponent.materialId, Renderer without MaterialKind
  - phase: 04-02
    provides: InputSystem independent of game, ActionMap
provides:
  - PlayerTorchComponent POD struct with visual parameters and computed lights
  - PlayerTorchSystem free function that updates torch lights each frame
  - GameOverlays namespace with game-specific ImGui overlay functions
  - RuntimeSceneRenderer::collectLights as a generic ECS light collector
  - ImGuiLayer as a clean engine-only ImGui lifecycle manager
affects:
  - 04-04
  - any phase adding new light types
  - any phase adding new game overlays

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Game constants extracted into ECS component fields, computed each frame by free function"
    - "Free function system pattern: updatePlayerTorch(registry, deltaTime) called from RuntimeGameSession::tick"
    - "Engine/game overlay separation via GameOverlays namespace in game layer"

key-files:
  created:
    - src/game/components/PlayerTorchComponent.h
    - src/game/systems/PlayerTorchSystem.h
    - src/game/systems/PlayerTorchSystem.cpp
    - src/game/ui/GameOverlays.h
    - src/game/ui/GameOverlays.cpp
  modified:
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/engine/ui/ImGuiLayer.h
    - src/engine/ui/ImGuiLayer.cpp
    - src/game/systems/RenderSystem.cpp
    - src/game/CMakeLists.txt
    - apps/level_editor/main.cpp

key-decisions:
  - "PlayerTorchComponent uses vector<RenderLight> computedLights field written by updatePlayerTorch each frame; collectLights just iterates this vector"
  - "innerConeDegrees/outerConeDegrees now live on PlayerTorchComponent (not DebugParams); DebugParams keeps its torch cone slider fields but they are no longer read by collectLights"
  - "GameOverlays lives in game layer (gameplay CMake target) since it depends on game types; level_editor/main.cpp updated as an additional caller"

patterns-established:
  - "ECS component for game-specific renderer data: component holds pre-computed values, system updates them, renderer reads them"
  - "Engine overlay boundary: ImGuiLayer only manages lifecycle and engine debug overlay; all game HUD/overlay code lives in game/ui/GameOverlays"

requirements-completed:
  - ENG-BOUNDARY-05
  - ENG-BOUNDARY-06

# Metrics
duration: 8min
completed: 2026-03-29
---

# Phase 04 Plan 03: Engine/Game Boundary — Torch and Overlays Summary

**Player torch extracted to PlayerTorchComponent+PlayerTorchSystem and game ImGui overlays moved from engine ImGuiLayer to GameOverlays namespace, making RuntimeSceneRenderer a generic ECS light collector and ImGuiLayer a clean engine-only lifecycle manager**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-29T17:32:37Z
- **Completed:** 2026-03-29T17:40:20Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments
- Extracted ~250 lines of hardcoded player torch constants, flicker math, and 4-light computation from RuntimeSceneRenderer into PlayerTorchComponent (POD) and PlayerTorchSystem (free function)
- RuntimeSceneRenderer::collectLights is now a generic ECS light collector — iterates PlayerTorchComponent.computedLights and LightComponent entities with no hardcoded game knowledge
- Moved renderMovementOverlay, renderViewmodelOverlay, renderInteractionPrompt, renderInventory from engine ImGuiLayer to game/ui/GameOverlays namespace, removing all game-specific forward declarations from the engine header

## Task Commits

Each task was committed atomically:

1. **Task 1: Create PlayerTorchComponent and PlayerTorchSystem, extract torch logic from RuntimeSceneRenderer** - `aa676b9` (refactor)
2. **Task 2: Move game-specific overlay methods from ImGuiLayer to game-layer GameOverlays module** - `d87501f` (refactor)

## Files Created/Modified
- `src/game/components/PlayerTorchComponent.h` - POD component with torch visual parameters and computed lights vector
- `src/game/systems/PlayerTorchSystem.h` - Declaration of updatePlayerTorch free function
- `src/game/systems/PlayerTorchSystem.cpp` - Torch flicker math and 4-light computation, updatePlayerTorch implementation
- `src/game/ui/GameOverlays.h` - Game-specific ImGui overlay function declarations in GameOverlays namespace
- `src/game/ui/GameOverlays.cpp` - Implementations of movement, viewmodel, interaction prompt, and inventory overlays
- `src/game/rendering/RuntimeSceneRenderer.cpp` - Removed torch constants/functions, collectLights now reads PlayerTorchComponent
- `src/game/runtime/RuntimeGameSession.cpp` - Added updatePlayerTorch after updateRuntimeCamera in tick(); attaches PlayerTorchComponent in rebuild()
- `src/engine/ui/ImGuiLayer.h` - Removed 6 game forward declarations and 4 static method declarations
- `src/engine/ui/ImGuiLayer.cpp` - Removed 4 game overlay implementations and their game-specific includes
- `src/game/systems/RenderSystem.cpp` - Updated calls to use GameOverlays:: instead of ImGuiLayer::
- `src/game/CMakeLists.txt` - Added PlayerTorchSystem.cpp and ui/GameOverlays.cpp to gameplay target
- `apps/level_editor/main.cpp` - Updated 2 overlay calls to use GameOverlays::

## Decisions Made
- PlayerTorchComponent stores `computedLights` vector written each frame by updatePlayerTorch; collectLights just reads it — clean separation between update logic and rendering
- innerConeDegrees/outerConeDegrees moved from DebugParams to PlayerTorchComponent; DebugParams torch cone slider fields remain but are no longer read by collectLights (they are orphaned sliders, will be cleaned up in a future plan)
- GameOverlays lives in the gameplay CMake target (not a separate library) since it depends on multiple game types

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Updated level_editor/main.cpp to call GameOverlays::**
- **Found during:** Task 2 (move overlay methods)
- **Issue:** Plan specified updating RenderSystem.cpp and RuntimeGameplay.cpp callers, but level_editor/main.cpp also called ImGuiLayer::renderInventory and ImGuiLayer::renderInteractionPrompt — not mentioned in plan
- **Fix:** Added `#include "game/ui/GameOverlays.h"` and updated 2 calls to GameOverlays:: in apps/level_editor/main.cpp
- **Files modified:** apps/level_editor/main.cpp
- **Verification:** Build succeeded after fix
- **Committed in:** d87501f (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing caller — Rule 2)
**Impact on plan:** Essential fix for build correctness. No scope creep.

## Issues Encountered
None beyond the level_editor caller discovered during Task 2.

## Next Phase Readiness
- RuntimeSceneRenderer is now a generic ECS light collector ready for further engine/game decoupling
- ImGuiLayer is engine-only, ready for any future engine refactoring without game concerns
- PlayerTorchComponent pattern established for attaching game-specific render data to entities

---
*Phase: 04-make-engine-fully-generic*
*Completed: 2026-03-29*

## Self-Check: PASSED

- FOUND: src/game/components/PlayerTorchComponent.h
- FOUND: src/game/systems/PlayerTorchSystem.h
- FOUND: src/game/systems/PlayerTorchSystem.cpp
- FOUND: src/game/ui/GameOverlays.h
- FOUND: src/game/ui/GameOverlays.cpp
- FOUND: commit aa676b9 (Task 1)
- FOUND: commit d87501f (Task 2)
