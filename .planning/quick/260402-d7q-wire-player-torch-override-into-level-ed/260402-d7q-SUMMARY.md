---
phase: quick
plan: 260402-d7q
subsystem: rendering/editor
tags: [torch, lighting, editor, environment, serialization]
dependency_graph:
  requires: []
  provides: [torch-params-in-env-files, torch-editor-ui]
  affects: [RenderLight.h, RuntimeLightingOverride.h, EnvironmentDefinition, EnvironmentDebugSync, EditorEnvironmentPanel]
tech_stack:
  added: []
  patterns: [env-file-key-value serialization, trackEnvItem undo pattern]
key_files:
  created: []
  modified:
    - src/engine/rendering/lighting/RenderLight.h
    - src/game/rendering/RuntimeLightingOverride.h
    - src/game/rendering/EnvironmentDefinition.cpp
    - src/game/rendering/EnvironmentDebugSync.cpp
    - src/editor/ui/EditorEnvironmentPanel.cpp
    - src/game/runtime/RuntimeGameSession.cpp
decisions:
  - PlayerTorchOverride moved to engine layer (RenderLight.h) so LightingEnvironment can embed it without game-layer include
  - torch_* serialization keys follow exact sun_*/fill_* naming convention for consistency
  - trackEnvItem + captureState pattern for undo used for every single torch control
metrics:
  duration_minutes: 15
  tasks_completed: 2
  files_modified: 6
  completed_date: "2026-04-02"
---

# Quick Task 260402-d7q: Wire Player Torch Override into Level Editor

**One-liner:** PlayerTorchOverride promoted to engine-layer LightingEnvironment field with 16 serialized keys and full editor UI with undo support.

## What Was Done

### Task 1: Move PlayerTorchOverride to engine layer and wire data flow (commit 2835d86)

**RenderLight.h** — `PlayerTorchOverride` struct inserted before `LightingEnvironment`. Struct is identical to the one previously in `RuntimeLightingOverride.h`. `LightingEnvironment` gains a `PlayerTorchOverride torch` field as its last member.

**RuntimeLightingOverride.h** — `PlayerTorchOverride` struct definition removed; replaced with a comment pointing to `RenderLight.h`. The `torch` member in `RuntimeLightingOverride` remains and now resolves via the existing `#include "engine/rendering/lighting/RenderLight.h"`.

**EnvironmentDefinition.cpp** — Load side: 16 `torch_*` keys parsed after `fill_intensity` block. Save side: 16 corresponding `writeBool`/`writeFloat`/`writeVec3` calls appended after `fill_intensity` serialization.

**EnvironmentDebugSync.cpp** — `applyEnvironmentSettings()` now copies `settings.lighting.torch` to `params.lighting.torch` alongside the existing sun/fill/hemisphere field copies. This completes the chain: `.env file -> LightingEnvironment -> EnvironmentRenderSettings -> DebugParams.lighting.torch -> RuntimeSceneRenderer`.

### Task 2: Add Player Torch controls to editor Environment panel (commit e51d45d)

**EditorEnvironmentPanel.cpp** — "Player Torch" `ImGui::TreeNodeEx` added in the Lighting collapsing header after the Fill tree node. 16 controls organized into four sections with `ImGui::SeparatorText`:
- Top-level: Torch Enabled (checkbox), Master Intensity
- Spotlight: Torch Color, Intensity, Radius, Inner Cone, Outer Cone
- Spill: Color, Intensity, Radius
- Halo: Color, Intensity, Radius
- Hand Glow: Color, Intensity, Radius

Every control uses the `beforeState = document.captureState()` + `trackEnvItem(beforeState, label, widget)` pattern for full undo/redo support. `##torch` ImGui ID suffixes prevent label collisions with Sun/Fill controls.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Remove stale initializeRuntimeDoors/updateRuntimeDoors calls from RuntimeGameSession.cpp**

- **Found during:** Task 1 build verification
- **Issue:** Working tree had `RuntimeGameplay.h` modified to remove `initializeRuntimeDoors`/`updateRuntimeDoors` declarations (incomplete door refactor towards `DoorAnimationSystem`), but `RuntimeGameSession.cpp` still called them. This caused a build error in the `gameplay` library, which `level-editor` depends on.
- **Fix:** Removed 4 calls (`initializeRuntimeDoors` x2, `updateRuntimeDoors` x2) from `RuntimeGameSession.cpp`. The `DoorAnimationSystem` class now handles door animation as a proper `System` subclass registered with `Application`.
- **Files modified:** `src/game/runtime/RuntimeGameSession.cpp`
- **Commit:** 2835d86 (bundled with Task 1)

## Source of Truth Chain (Complete)

```
.env file
  -> loadEnvironmentDefinitionAsset() (torch_* keys)
  -> LightingEnvironment.torch (in RenderLight.h)
  -> makeEnvironmentRenderSettings() (copies definition.lighting)
  -> applyEnvironmentSettings() (copies settings.lighting.torch to params.lighting.torch)
  -> DebugParams.lighting (RuntimeLightingOverride)
  -> RuntimeSceneRenderer reads params.lighting.torch
```

## Known Stubs

None. The serialization round-trip is complete: the editor panel writes to `LightingEnvironment.torch`, save/load persists all 16 torch_* keys to `.env` files, and the sync path delivers them to the runtime renderer.

## Self-Check: PASSED

- FOUND: src/engine/rendering/lighting/RenderLight.h (PlayerTorchOverride struct + torch field in LightingEnvironment)
- FOUND: src/game/rendering/RuntimeLightingOverride.h (struct removed, torch member retained)
- FOUND: src/game/rendering/EnvironmentDefinition.cpp (torch_enabled key in load and save)
- FOUND: src/game/rendering/EnvironmentDebugSync.cpp (params.lighting.torch = settings.lighting.torch)
- FOUND: src/editor/ui/EditorEnvironmentPanel.cpp (Player Torch tree node)
- FOUND commit 2835d86 (Task 1)
- FOUND commit e51d45d (Task 2)
- Both pixel-roguelike and level-editor build cleanly
