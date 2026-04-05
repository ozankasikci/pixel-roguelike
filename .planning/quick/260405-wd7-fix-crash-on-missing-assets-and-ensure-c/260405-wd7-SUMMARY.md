---
phase: quick
plan: 260405-wd7
subsystem: runtime, ci
tags: [crash-fix, error-handling, ci, macos, windows, artifact-packaging]
dependency_graph:
  requires: []
  provides: [graceful-asset-validation, macos-ci, fixed-windows-artifacts]
  affects: [apps/runtime/main.cpp, PathUtils]
tech_stack:
  added: []
  patterns: [early-validation, try-catch-safety-net]
key_files:
  created:
    - .github/workflows/build-macos.yml
  modified:
    - apps/runtime/main.cpp
    - src/engine/core/PathUtils.h
    - src/engine/core/PathUtils.cpp
    - .github/workflows/build-windows.yml
decisions:
  - Console-only error output (no platform-specific dialog boxes) for portability
  - hasValidProjectRoot() reuses resolveProjectPath() to leverage existing search logic
metrics:
  duration: 960s
  completed: 2026-04-05
  tasks_completed: 2
  tasks_total: 2
  files_changed: 5
---

# Quick Plan 260405-wd7: Fix Crash on Missing Assets and Ensure CI Artifact Layout

Early asset directory validation with clear error messaging, plus CI workflow fixes so downloaded artifacts work out of the box.

## Task Results

### Task 1: Add early asset validation and graceful error handling to runtime
**Commit:** c40f211

Added `hasValidProjectRoot()` to `PathUtils.h/.cpp` that checks whether `resolveProjectPath("assets")` resolves to an existing directory. In `main.cpp`, this check runs before `Application` construction -- if assets/ is missing, the game prints a clear error to both spdlog and stderr and exits with code 1. Additionally, `content.loadDefaults()` is now wrapped in a try/catch for `std::exception`, so individual missing asset files produce a readable error instead of an uncaught exception abort.

**Files:** `apps/runtime/main.cpp`, `src/engine/core/PathUtils.h`, `src/engine/core/PathUtils.cpp`

### Task 2: Fix Windows CI artifact layout and create macOS CI workflow
**Commit:** 59ffce6

Replaced the Windows workflow's direct multi-path `upload-artifact` with a staging step that copies `pixel-roguelike.exe` and `assets/` into a flat `staging/` directory before upload. This ensures the artifact extracts with exe and assets/ as siblings.

Created `.github/workflows/build-macos.yml` mirroring the Windows pattern: installs Homebrew deps (glfw, glm, spdlog, openal-soft), sets up Python 3.12 + jinja2 for GLAD, configures with `ci-macos` preset, builds `pixel-roguelike`, and packages binary + assets into a flat staging directory for artifact upload.

**Files:** `.github/workflows/build-windows.yml`, `.github/workflows/build-macos.yml`

## Deviations from Plan

None -- plan executed exactly as written.

## Known Stubs

None.

## Verification

1. `cmake --build build --target pixel-roguelike` -- PASSED (compiled successfully)
2. Both workflow YAML files parse as valid YAML -- PASSED
3. Windows workflow uses staging directory for flat artifact layout -- CONFIRMED
4. macOS workflow uses ci-macos preset and packages correctly -- CONFIRMED

## Self-Check: PASSED

- All 5 files exist on disk
- Commit c40f211 verified in git log
- Commit 59ffce6 verified in git log
