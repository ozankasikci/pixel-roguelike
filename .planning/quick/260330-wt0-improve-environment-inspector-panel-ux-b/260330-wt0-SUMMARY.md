---
phase: quick
plan: 260330-wt0
subsystem: editor-ui
tags: [editor, imgui, ux, environment-panel]
dependency_graph:
  requires: []
  provides: [grouped-environment-panel]
  affects: [src/editor/ui/EditorEnvironmentPanel.cpp]
tech_stack:
  added: []
  patterns: [ImGui::SeparatorText for sub-group headers within CollapsingHeader]
key_files:
  modified:
    - src/editor/ui/EditorEnvironmentPanel.cpp
decisions:
  - Omitted SSAO and Shadows SeparatorText groups from Post section because no SSAO/CSM fields exist in PostProcessParams yet; plan spec says same set of controls, different order — no controls added
  - Sky Layers TreeNode removed and its contents (Cloud Layer A, B, Horizon Layer) distributed into dedicated Clouds and Horizon SeparatorText groups as specified
metrics:
  duration: 10m
  completed_date: "2026-03-30"
  tasks_completed: 2
  files_modified: 1
---

# Quick Task 260330-wt0: Improve Environment Inspector Panel UX

One-liner: Reorganized Environment inspector Post and Sky sections into 6 SeparatorText sub-groups each, with enable checkboxes at the top of their related parameter groups.

## What Was Done

### Task 1: Reorganize Post section

The Post CollapsingHeader previously showed 30+ controls in a flat list with all enable checkboxes at the top followed by interleaved parameters. Reorganized into 6 logical groups using `ImGui::SeparatorText`:

1. **Tone Mapping** — Enable Tone Map, Tone Mapper combo, Exposure, Gamma, Contrast, Saturation
2. **Bloom** — Enable Bloom, Bloom Threshold, Bloom Intensity, Bloom Radius
3. **Fog** — Enable Fog, Fog Density, Fog Start, Depth View Scale, Fog Near Color, Fog Far Color
4. **Edges** — Enable Edges, Edge Threshold
5. **Color Grading** — Split Strength, Split Balance, Shadow Tint, Highlight Tint
6. **Effects** — Enable Sky, Enable Vignette + params, Enable Grain + param, Enable Scanlines + params, Enable Sharpen + param

Note: The plan specified SSAO and Shadows groups, but no SSAO fields (AO Radius/Bias/Strength) or CSM Split Blend field exist in `PostProcessParams`. Per the "same set, different order" constraint, those groups were omitted.

### Task 2: Reorganize Sky section

The Sky CollapsingHeader previously mixed sun, moon, panorama, cloud, and horizon controls at the same level, with a "Sky Layers" TreeNode containing cloud and horizon layer paths. Reorganized into 6 SeparatorText groups:

1. **Sky Colors** — Zenith, Horizon, Ground Haze colors
2. **Sun** — Sun Direction, Sun Color, Sun Size, Sun Glow
3. **Moon** — Moon Enabled, Moon Direction, Moon Color, Moon Size, Moon Glow
4. **Panorama** — Panorama Path, Panorama Tint, Panorama Strength, Panorama Yaw
5. **Cubemap** — (TreeNodeEx preserved as-is)
6. **Clouds** — Cloud Layer A, Cloud Layer B, Cloud Tint, Cloud Scale, Cloud Speed, Cloud Coverage, Cloud Parallax
7. **Horizon** — Horizon Layer, Horizon Tint, Horizon Height, Horizon Fade

The "Sky Layers" TreeNode was removed; its contents distributed into Clouds and Horizon groups.

## Deviations from Plan

**1. [Rule 2 - Missing items] Post section SSAO and Shadows groups omitted**
- **Found during:** Task 1 implementation
- **Issue:** Plan specified SSAO group (AO Radius, AO Bias, AO Strength) and Shadows group (CSM Split Blend) but these fields do not exist in `PostProcessParams`. The plan constraint states "Do NOT remove or add any controls — same set, different order."
- **Fix:** Omitted both empty groups. Post section has 6 groups instead of the planned 8.
- **Files modified:** None (decision to not add non-existent controls)

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| Task 1 + Task 2 | d28fcd2 | Reorganize Environment inspector panel Post and Sky sections |

## Self-Check

- [x] `/Users/ozan/Projects/gsd-3d-roguelike/.claude/worktrees/agent-ae6a8a3c/src/editor/ui/EditorEnvironmentPanel.cpp` — modified and committed
- [x] Build: `cmake --build build --target level-editor` — succeeded with no errors or warnings
- [x] Commit d28fcd2 exists

## Self-Check: PASSED
