---
phase: 15-prototype-editor-imgui-themes-and-choose-a-final-skin-direction
plan: 01
subsystem: ui
tags: [imgui, editor, theme, level-editor]
requires:
  - phase: 14-improve-lighting-reflections-occlusion-and-shadow-quality
    provides: shared editor preview quality/menu infrastructure carried into the level editor
provides:
  - ImGui theme preset API in ImGuiLayer
  - three repo-local editor chrome presets built on upstream Dear ImGui styles
  - live View -> Interface Theme switching in the level editor
affects: [15-02-PLAN.md, editor-ui-preferences, level-editor]
tech-stack:
  added: []
  patterns: [ImGui stock-style-plus-delta presets, pending global style application before ImGui::NewFrame]
key-files:
  created: [.planning/phases/15-prototype-editor-imgui-themes-and-choose-a-final-skin-direction/15-01-SUMMARY.md]
  modified: [src/engine/ui/ImGuiLayer.h, src/engine/ui/ImGuiLayer.cpp, apps/level_editor/main.cpp]
key-decisions:
  - "Theme presets stay repo-local in ImGuiLayer and start from Dear ImGui Dark/Light baselines instead of introducing a skinning framework."
  - "The editor exposes theme comparison only through View -> Interface Theme, matching the existing Interface Font workflow."
patterns-established:
  - "Global ImGui chrome changes are requested via pending preset state and applied before ImGui::NewFrame."
  - "Editor theme options are modeled as fixed named presets, not freeform style editing."
requirements-completed: []
duration: 8min
completed: 2026-04-04
---

# Phase 15 Plan 01: Add ImGui theme preset infrastructure and a live View -> Interface Theme selector Summary

**Three live-switchable Dear ImGui editor themes wired through ImGuiLayer and exposed from the level editor View menu**

## Performance

- **Duration:** 8 min
- **Started:** 2026-04-04T12:10:20Z
- **Completed:** 2026-04-04T12:18:07Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Added `ImGuiThemePreset`, labels, request/state accessors, and pending theme application to [`ImGuiLayer.h`](/Users/ozan/Projects/gsd-3d-roguelike/src/engine/ui/ImGuiLayer.h).
- Implemented `Warm Studio Dark`, `Spectrum-Inspired Dark`, and `Soft Light Tooling` as repo-local style deltas on top of upstream Dear ImGui dark/light baselines in [`ImGuiLayer.cpp`](/Users/ozan/Projects/gsd-3d-roguelike/src/engine/ui/ImGuiLayer.cpp).
- Added a live `View -> Interface Theme` submenu in [`main.cpp`](/Users/ozan/Projects/gsd-3d-roguelike/apps/level_editor/main.cpp) so the editor can swap themes without restart.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add repo-local ImGui theme presets and pre-frame theme application to ImGuiLayer** - `5033dec` (feat)
2. **Task 2: Add a live View -> Interface Theme menu that switches between the three approved presets** - `7c14952` (feat)

## Files Created/Modified
- [`src/engine/ui/ImGuiLayer.h`](/Users/ozan/Projects/gsd-3d-roguelike/src/engine/ui/ImGuiLayer.h) - Declares the theme preset enum, labels, request API, and pending/live theme state.
- [`src/engine/ui/ImGuiLayer.cpp`](/Users/ozan/Projects/gsd-3d-roguelike/src/engine/ui/ImGuiLayer.cpp) - Defines the three theme presets, shared chrome application logic, and pre-frame theme application path.
- [`apps/level_editor/main.cpp`](/Users/ozan/Projects/gsd-3d-roguelike/apps/level_editor/main.cpp) - Adds the View menu theme selector alongside the existing font preset menu.

## Decisions Made

- Kept theme ownership in `ImGuiLayer` so global base style changes remain engine-layer concerns and existing panel-local style overrides still layer on top safely.
- Used upstream `StyleColorsDark()` and `StyleColorsLight()` as the preset baselines, then applied repo-local palette/spacing deltas for the approved theme directions.
- Inserted theme selection directly into the existing View menu instead of adding a separate theming/debug surface.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Replaced unavailable `ImLerp` calls with a local color interpolation helper**
- **Found during:** Task 1 (Add repo-local ImGui theme presets and pre-frame theme application to ImGuiLayer)
- **Issue:** The initial preset implementation assumed `ImLerp` was available from the public ImGui headers in this translation unit, but the target failed to compile.
- **Fix:** Added a small file-local `lerpColor()` helper in `ImGuiLayer.cpp` and routed all derived theme colors through it.
- **Files modified:** `src/engine/ui/ImGuiLayer.cpp`
- **Verification:** `cmake --build build --target level-editor`
- **Committed in:** `5033dec`

---

**Total deviations:** 1 auto-fixed (1 blocking issue)
**Impact on plan:** No scope change. The fix was required to make the planned theme implementation compile cleanly against the pinned Dear ImGui version.

## Issues Encountered

- `STATE.md` contained pre-existing merge markers in the Decisions section. They were removed while preserving both sides' accumulated planning notes so the required metadata commit could land cleanly.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

The editor now has the live comparison surface needed to evaluate the three approved themes in-context. Phase `15-02` can focus on local font/theme preference persistence and selecting the final default skin direction without changing the switching architecture.

## Self-Check: PASSED

- Found summary file: `.planning/phases/15-prototype-editor-imgui-themes-and-choose-a-final-skin-direction/15-01-SUMMARY.md`
- Found task commits: `5033dec`, `7c14952`
