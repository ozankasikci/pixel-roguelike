---
phase: quick
plan: 260402-4is
subsystem: game/rendering, engine/ui
tags: [lighting, debug, imgui, torch, runtime-tuning]
dependency_graph:
  requires: []
  provides: [PlayerTorchOverride, runtime-torch-debug-ui]
  affects: [RuntimeLightingOverride, RuntimeSceneRenderer, ImGuiLayer]
tech_stack:
  added: []
  patterns: [DebugParams override pattern, BeginDisabled/EndDisabled toggle]
key_files:
  created: []
  modified:
    - src/game/rendering/RuntimeLightingOverride.h
    - src/game/rendering/RuntimeSceneRenderer.cpp
    - src/engine/ui/ImGuiLayer.cpp
decisions:
  - "Positional offset constants (kPlayerTorchForwardOffset etc.) kept as constexpr — not useful to tune at runtime"
  - "masterIntensity multiplier applies to intensity only, not color or radius — matches expected UX"
  - "Early break on !torch.enabled skips all 4 torch lights without entering the loop body"
metrics:
  duration: 5min
  completed: "2026-04-02T00:22:43Z"
  tasks_completed: 2
  files_modified: 3
---

# Phase quick Plan 260402-4is: Player Torch Debug Overlay Summary

Player torch light parameters wired into runtime ImGui debug overlay via PlayerTorchOverride struct, replacing hardcoded constexpr values for color, intensity, radius, and cone angles across all 4 torch lights.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Add PlayerTorchOverride struct and wire into collectLights | 24a04cb | RuntimeLightingOverride.h, RuntimeSceneRenderer.cpp |
| 2 | Add ImGui controls for player torch in Lighting section | c7de887 | ImGuiLayer.cpp |

## What Was Built

### PlayerTorchOverride struct (RuntimeLightingOverride.h)
New struct added before `RuntimeLightingOverride` with default values matching the old constexpr constants exactly:
- `enabled` bool + `masterIntensity` float (scales all 4 lights together)
- Main spotlight: `torchColor`, `torchIntensity`, `torchRadius`, `torchInnerConeDegrees`, `torchOuterConeDegrees`
- Spill light: `spillColor`, `spillIntensity`, `spillRadius`
- Halo: `haloColor`, `haloIntensity`, `haloRadius`
- Hand glow: `handGlowColor`, `handGlowIntensity`, `handGlowRadius`

Added as `PlayerTorchOverride torch` member on `RuntimeLightingOverride`.

### collectLights changes (RuntimeSceneRenderer.cpp)
- Removed 14 tunable constexpr constants (color, intensity, radius, cone angles for all 4 lights)
- Kept 7 positional offset constexpr constants unchanged
- Added early `break` when `!params.lighting.torch.enabled` to skip all torch lights
- All light construction reads from `params.lighting.torch.*`; intensity of each light multiplied by `torch.masterIntensity`

### ImGui Lighting > Player Torch tree node (ImGuiLayer.cpp)
Tree node added after the existing Sun/Fill directional tree nodes:
- Torch Enabled checkbox with BeginDisabled guard
- Master Intensity slider (0-3)
- Spotlight section: color picker, intensity (0-2), radius (0.5-15), inner cone (5-90 deg), outer cone (10-120 deg)
- Spill Light section: color picker, intensity (0-8), radius (1-20)
- Halo section: color picker, intensity (0-5), radius (1-15)
- Hand Glow section: color picker, intensity (0-0.5), radius (0.2-5)

## Deviations from Plan

None - plan executed exactly as written.

## Self-Check: PASSED

Files verified:
- FOUND: src/game/rendering/RuntimeLightingOverride.h (contains PlayerTorchOverride struct)
- FOUND: src/game/rendering/RuntimeSceneRenderer.cpp (contains params.lighting.torch references)
- FOUND: src/engine/ui/ImGuiLayer.cpp (contains Player Torch tree node)

Commits verified:
- FOUND: 24a04cb (Task 1)
- FOUND: c7de887 (Task 2)

Build: pixel-roguelike compiled clean with no errors or warnings.
