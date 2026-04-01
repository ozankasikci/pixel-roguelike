# Phase 10: Global Keyboard Shortcuts and Hover Highlight - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-01
**Phase:** 10-global-keyboard-shortcuts-and-hover-highlight
**Areas discussed:** Hover highlight style, Hover highlight behavior, Shortcut text safety, Frame camera (F key)

---

## Hover Highlight Style

| Option | Description | Selected |
|--------|-------------|----------|
| Subtle wireframe | Same bounding-box wireframe as selection, thinner (2px) and cool color (light blue/white) at ~60% opacity | ✓ |
| Tinted overlay | Semi-transparent color tint on mesh surface. More polished but different rendering approach | |
| Outline glow | Soft bloom glow around silhouette edges. Most polished but needs silhouette detection shaders | |

**User's choice:** Subtle wireframe
**Notes:** Same shape language as selection, visually lightweight

### Hover Depth Approach

| Option | Description | Selected |
|--------|-------------|----------|
| Depth-tested only | Only show hover where object is visible. No ghost pass for occluded parts | ✓ |
| Two-pass like selection | Mirror Phase 9's ghost + primary approach. Consistent but noisier | |

**User's choice:** Depth-tested only
**Notes:** Simpler, less visual noise than selection's two-pass approach

---

## Hover Highlight Behavior

### Hover on Selected Objects

| Option | Description | Selected |
|--------|-------------|----------|
| No hover on selected | Already-selected objects show gold selection only, no stacked hover. Matches Unity/Unreal | ✓ |
| Always show hover | Hover appears on any object under cursor including selected ones | |

**User's choice:** No hover on selected
**Notes:** None

### Suppression Conditions

| Option | Description | Selected |
|--------|-------------|----------|
| During orbit/pan | Suppress when right-mouse orbiting or middle-mouse panning | ✓ |
| During gizmo drag | Suppress when actively dragging translate/rotate/scale gizmo | ✓ |
| During placement mode | Suppress when placing a new mesh from asset browser | ✓ |
| During play preview | Suppress when gameplay preview is active and captured | ✓ |

**User's choice:** All four suppression conditions selected
**Notes:** None

---

## Shortcut Text Safety

| Option | Description | Selected |
|--------|-------------|----------|
| Guard all single-key shortcuts | Delete, F, W, E, R, Escape suppressed when WantTextInput. Ctrl combos still work. Matches Unity | ✓ |
| Guard all shortcuts | All shortcuts including Ctrl combos suppressed in text fields | |
| Guard only Delete and letters | Only guard Delete/F/W/E/R, let Escape always work, let Ctrl combos always work | |

**User's choice:** Guard all single-key shortcuts
**Notes:** Escape deselects text field first (ImGui default), next Escape clears selection

---

## Frame Camera (F Key)

### Animation

| Option | Description | Selected |
|--------|-------------|----------|
| Smooth animation | Camera lerps to framing position over ~0.3s with ease-out | ✓ |
| Instant snap | Camera teleports immediately. Current behavior | |
| You decide | Claude picks based on implementation complexity | |

**User's choice:** Smooth animation
**Notes:** ~0.3s matches Unity/Unreal feel

### Multi-Selection

| Option | Description | Selected |
|--------|-------------|----------|
| Single only | F only works with exactly one object selected. Current behavior | |
| Frame all selected | Computes combined bounding box, frames to fit all. Matches Unity | ✓ |
| You decide | Claude picks based on what makes sense | |

**User's choice:** Frame all selected
**Notes:** None

---

## Claude's Discretion

- Specific lerp/easing function for camera animation
- Whether hover raycasting reuses pickEditorObjects directly or uses lighter variant
- ImGuiInputFlags_RouteGlobal vs current IsKeyPressed approach

## Deferred Ideas

None — discussion stayed within phase scope.
