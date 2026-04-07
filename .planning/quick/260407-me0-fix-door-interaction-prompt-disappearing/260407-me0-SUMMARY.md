---
phase: quick
plan: 260407-me0
subsystem: game/runtime
tags: [door, interaction, ECS, bug-fix]
dependency_graph:
  requires: []
  provides: [door-interactable-reset-on-closed]
  affects: [RuntimeGameplay, InteractableComponent, DoorComponent]
tech_stack:
  added: []
  patterns: [try_get-guard-before-continue]
key_files:
  modified:
    - src/game/runtime/RuntimeGameplay.cpp
decisions:
  - Reset InteractableComponent inline in the closed-door early-exit branch, matching DoorAnimationSystem::update() pattern
metrics:
  duration: "5 minutes"
  completed: "2026-04-07"
  tasks_completed: 1
  files_modified: 1
---

# Quick 260407-me0: Fix door interaction prompt disappearing — Summary

**One-liner:** Added InteractableComponent reset (enabled=true, busy=false) to the closed-door early-exit branch in updateRuntimeDoorAnimation, matching the existing DoorAnimationSystem::update() pattern.

## What Was Done

`updateRuntimeDoorAnimation()` in `RuntimeGameplay.cpp` had an early `continue` for doors that are neither opening nor opened (i.e., closed). After `restoreBaselineState()` resets door state to closed between play sessions, the animation function would skip those entities entirely — never restoring `InteractableComponent.enabled` to `true`. This caused the "Press E to open" prompt to disappear after a play-stop-play cycle.

The fix inserts a `try_get<InteractableComponent>` guard before the `continue`, resetting `busy=false` and `enabled=true`. This matches the identical pattern already present in `DoorAnimationSystem::update()` (lines 105-108).

## Tasks

| # | Name | Commit | Files |
|---|------|--------|-------|
| 1 | Reset InteractableComponent for closed doors | c1afd64 | src/game/runtime/RuntimeGameplay.cpp |

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED

- File modified: `src/game/runtime/RuntimeGameplay.cpp` — FOUND
- Commit c1afd64 — FOUND
- Build: `[100%] Built target pixel-roguelike` — PASSED
