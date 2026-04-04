# Phase 15: Prototype Editor ImGui Themes and Choose a Final Skin Direction - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-04
**Phase:** 15-prototype-editor-imgui-themes-and-choose-a-final-skin-direction
**Areas discussed:** Theme Architecture, Preset Set, Styling Scope, Editor Workflow

---

## Theme Architecture

| Option | Description | Selected |
|--------|-------------|----------|
| Repo-local preset code | Keep upstream Dear ImGui, implement a few theme functions in-project | ✓ |
| External theme browser dependency | Pull in a theme package/tool as a runtime dependency | |
| ImGui fork adoption | Base the editor skin on a maintained fork such as a Spectrum-oriented fork | |

**User's choice:** Auto mode selected the recommended default: repo-local preset code on top of upstream Dear ImGui.
**Notes:** This matches the current code structure, keeps maintenance low, and avoids turning a visual experiment into a dependency migration.

---

## Preset Set

| Option | Description | Selected |
|--------|-------------|----------|
| Warm Studio Dark + Spectrum-Inspired Dark + Soft Light Tooling | Prototype three distinct directions with one likely default, one professional dark reference, and one light contrast check | ✓ |
| Dark-only exploration | Prototype only dark themes to minimize scope | |
| One final theme only | Skip side-by-side comparison and implement a single chosen direction immediately | |

**User's choice:** Auto mode selected the recommended default: prototype three presets.
**Notes:** Three presets are enough to compare direction without making the phase turn into a theme gallery.

---

## Styling Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Global chrome polish only | Focus on colors, spacing, borders, tabs, menus, buttons, and readability | ✓ |
| Full widget redesign | Custom draw widgets and heavily restyle Dear ImGui controls | |
| Minimal color swap only | Keep all spacing/radii defaults and only change colors | |

**User's choice:** Auto mode selected the recommended default: global chrome polish only.
**Notes:** This gives a meaningful visual upgrade while staying compatible with the current editor and avoiding a custom UI framework.

---

## Editor Workflow

| Option | Description | Selected |
|--------|-------------|----------|
| Live theme switcher in View menu | Add `View -> Interface Theme` next to font controls for instant comparison | ✓ |
| Startup-only theme choice | Pick a theme at startup or in config, no live switching | |
| Temporary debug-only toggle | Hide theme selection in a debug panel instead of the main editor UI | |

**User's choice:** Auto mode selected the recommended default: live theme switcher in the View menu.
**Notes:** The editor already exposes live font switching there, so the same interaction model is the least surprising.

---

## the agent's Discretion

- Exact color values, spacing constants, and radii for each preset
- Persistence implementation details for saving the selected theme
- Whether to surface extra theme metadata in help UI beyond the menu selector

## Deferred Ideas

- User-importable custom themes
- Adopting an external ImGui fork as the long-term foundation
- Restyling runtime/HUD UI to match the editor chrome
