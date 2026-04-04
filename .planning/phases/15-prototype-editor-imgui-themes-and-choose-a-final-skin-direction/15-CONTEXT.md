# Phase 15: Prototype Editor ImGui Themes and Choose a Final Skin Direction - Context

**Gathered:** 2026-04-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Prototype 2-3 editor-only Dear ImGui theme presets inside the existing level editor, compare them live, and converge on one final skin direction that better matches the project’s warm, polished tool aesthetic. This phase is about the editor chrome and usability feel, not a broader UX redesign, custom widget framework, or a fork of Dear ImGui.

</domain>

<decisions>
## Implementation Decisions

### Theme Architecture
- **D-01:** Build the theme system as repo-local `ImGuiStyle` preset code on top of upstream Dear ImGui. Do not adopt an ImGui fork for this phase.
- **D-02:** Keep the implementation lightweight and reversible: a small set of named theme functions plus menu-driven switching is preferred over a large skinning framework.
- **D-03:** Theme changes should apply globally through the existing `ImGuiLayer` initialization/frame path so the whole editor benefits consistently.

### Presets To Prototype
- **D-04:** Prototype exactly three presets in the first pass:
  - `Warm Studio Dark`
  - `Spectrum-Inspired Dark`
  - `Soft Light Tooling`
- **D-05:** `Warm Studio Dark` is the recommended target aesthetic and likely final default unless live testing clearly favors another preset.
- **D-06:** `Spectrum-Inspired Dark` is included as the “professional tools” reference point, not as a commitment to mirror Adobe Spectrum exactly.
- **D-07:** `Soft Light Tooling` is included as a contrast check to validate whether the editor benefits from a lighter palette, but it is not the expected default.

### Styling Scope
- **D-08:** Focus on the parts that most affect everyday editor readability and feel: window backgrounds, panels, headers, menus, tabs, buttons, separators, borders, selection accents, rounding, spacing, and frame padding.
- **D-09:** Do not build custom-drawn widgets, animated skins, or a bespoke retained-mode UI layer in this phase.
- **D-10:** Keep the viewport itself visually subordinate to the scene content. The editor chrome should improve readability without fighting the 3D view.

### Editor Workflow
- **D-11:** Add a live `View -> Interface Theme` selector, parallel to the existing `Interface Font` menu, so themes can be compared without restarting the editor.
- **D-12:** The chosen theme should be persisted in the same spirit as the existing font preset selection and other editor UI preferences.
- **D-13:** Theme switching should be safe with the existing local `PushStyleColor` / `PushStyleVar` overrides already used by some panels and overlays.

### Visual Direction
- **D-14:** Preserve the project’s established art direction in the tool UI: warm, muted, clean, and slightly institutional rather than neon, gamer, or ultra-minimal debug-console styling.
- **D-15:** Favor readable contrast and hierarchy over novelty. The editor should feel deliberate and professional, not flashy.
- **D-16:** Keep the default dark direction warmer and less blue/purple than stock ImGui.

### the agent's Discretion
- Exact palette values, corner radii, border strengths, padding, and spacing per preset
- Whether to expose theme preview details in the shortcuts/help popup or keep the selector only in the View menu
- Whether the final implementation stores the theme alongside font preference in an existing config path or a new editor UI preference field

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project and milestone direction
- `.planning/PROJECT.md` — Core art direction and editor milestone framing; warm, muted, Stanley Parable-inspired aesthetic is non-negotiable
- `.planning/ROADMAP.md` — Phase 15 scope anchor and milestone placement
- `.planning/phases/08-create-institutional-room-scene-from-concept-art/08-CONTEXT.md` — Prior color/atmosphere decisions for the game’s warm institutional look
- `.planning/phases/10-global-keyboard-shortcuts-and-hover-highlight/10-CONTEXT.md` — Existing editor visual language decisions, especially subtle/high-clarity overlay styling
- `.planning/phases/14-improve-lighting-reflections-occlusion-and-shadow-quality/14-CONTEXT.md` — Recent visual-direction decisions preserving warm soft lighting, clean geometry, and muted color palette

### Existing editor UI implementation
- `src/engine/ui/ImGuiLayer.h` — Current ImGui global UI entry point, font preset state, and likely theme integration surface
- `src/engine/ui/ImGuiLayer.cpp` — Current `ImGui::StyleColorsDark()` baseline and font preset loading path
- `apps/level_editor/main.cpp` — Existing `View` menu, live `Interface Font` menu, and editor-level UI controls where theme selection should integrate

### Codebase guidance
- `.planning/codebase/CONVENTIONS.md` — Naming/style conventions for introducing theme types and helper functions
- `.planning/codebase/STRUCTURE.md` — Current ownership boundaries between engine/editor/app layers
- `.planning/codebase/STACK.md` — Dear ImGui docking branch usage and dependency constraints

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ImGuiLayer` already owns the global Dear ImGui context setup and currently applies the stock dark theme via `ImGui::StyleColorsDark()`. This is the cleanest place to insert named theme presets.
- The editor already has a live font preset system (`requestFontPreset()` and the `Interface Font` menu), which provides a strong precedent for a parallel theme preset system.
- The `View` menu in `apps/level_editor/main.cpp` already exposes interface-related controls, so `Interface Theme` can live there without introducing a new settings surface.

### Established Patterns
- Global UI configuration is centralized in `src/engine/ui/ImGuiLayer.cpp`, while editor-specific choice surfaces live in `apps/level_editor/main.cpp`.
- Small local visual exceptions are already implemented with scoped `PushStyleColor` / `PushStyleVar` calls in the editor and overlays, so the global theme must remain compatible with these local overrides.
- Font presets are data-like named options rather than freeform file pickers. Theme presets should follow the same model.

### Integration Points
- `src/engine/ui/ImGuiLayer.h/.cpp` — add theme enum/state, labels, preset application function, and pending/live switching path
- `apps/level_editor/main.cpp` — add `View -> Interface Theme` menu and wire current selection through the editor frame loop
- Any persistence path already used for editor UI preferences or layout/font state — likely the best home for saving the chosen theme

</code_context>

<specifics>
## Specific Ideas

- `Warm Studio Dark` should use charcoal/warm-gray panels with restrained amber/olive accents rather than stock ImGui’s colder blue-gray feel.
- `Spectrum-Inspired Dark` should borrow the “professional creative tool” feel: tighter contrast hierarchy, calmer controls, and clearer active/hover states.
- `Soft Light Tooling` should feel intentional and paper-like, not default bright white. It exists mainly to test whether a lighter editor is actually preferable in this project.
- The theme system should be easy to compare live, so choosing a final skin becomes a visual decision in-editor rather than an abstract code debate.

</specifics>

<deferred>
## Deferred Ideas

- Adopting a full external ImGui fork or heavy third-party skin framework
- Building a full editor branding system beyond colors/spacing/rounding
- Runtime/game HUD restyling to match the editor theme
- Arbitrary user-authored custom themes or import/export of theme packs

</deferred>

---

*Phase: 15-prototype-editor-imgui-themes-and-choose-a-final-skin-direction*
*Context gathered: 2026-04-04*
