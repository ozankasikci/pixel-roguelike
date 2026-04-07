# Quick Task 260407-4qf: Door open state persists across play sessions

**Date:** 2026-04-07
**Commit:** 8564440

## Problem

Opening a door in play mode, then stopping and restarting play mode, left the door in its opened state. The door's runtime state (DoorComponent progress/opened fields) was not being reset when play mode stopped.

## Root Cause

In `apps/level_editor/main.cpp`, the play-toggle-off handler only called `endCapture()` and marked the dirty state as `GameplayStateReset`. The actual `resetForPlay()` call was deferred until the next play start, leaving modified door state visible in the editor preview.

## Fix

Call `runtimePreviewSession.resetForPlay()` immediately when play mode stops. Clear dirty state to `None` after the reset, unless a higher-priority state (`FullWorldRebuild` or `EnvironmentOnly`) is pending from scene edits made during play.

## Files Changed

| File | Change |
|------|--------|
| `apps/level_editor/main.cpp` | Immediate `resetForPlay()` on play stop, smart dirty state management |
