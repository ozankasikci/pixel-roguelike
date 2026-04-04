# Phase 15: Prototype Editor ImGui Themes and Choose a Final Skin Direction - Research

**Researched:** 2026-04-04
**Domain:** Dear ImGui editor theming, live preset switching, and editor-local UI preference persistence
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
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

### Claude's Discretion
- Exact palette values, corner radii, border strengths, padding, and spacing per preset
- Whether to expose theme preview details in the shortcuts/help popup or keep the selector only in the View menu
- Whether the final implementation stores the theme alongside font preference in an existing config path or a new editor UI preference field

### Deferred Ideas (OUT OF SCOPE)
- Adopting a full external ImGui fork or heavy third-party skin framework
- Building a full editor branding system beyond colors/spacing/rounding
- Runtime/game HUD restyling to match the editor theme
- Arbitrary user-authored custom themes or import/export of theme packs
</user_constraints>

## Project Constraints (from CLAUDE.md)

- Preserve the project art direction: Stanley Parable-inspired, warm, muted, clean, slightly institutional, and never neon/gamer-styled.
- Stay inside the custom C++ engine and OpenGL 4.1 / GLSL 4.10 constraints.
- Respect the layer direction `Engine -> Game -> Editor`; theme application belongs in `src/engine/ui`, while editor persistence and menus belong in editor/app code.
- Follow repo conventions: `PascalCase` types/files, `camelCase` functions, `snake_case` locals, `#pragma once`, and project includes relative to `src/`.
- Tests use standalone executables and existing CMake helpers, not a third-party test framework.
- GSD workflow enforcement applies to future implementation work.
- No project-specific skills are currently defined under `.claude/skills/` or `.agents/skills/`.

## Summary

This phase should be planned as a small editor-only extension of the existing `ImGuiLayer` pattern, not as a theming subsystem. The repo already has the right seams: `src/engine/ui/ImGuiLayer.cpp` owns global Dear ImGui setup, `apps/level_editor/main.cpp` already exposes a live `View -> Interface Font` selector, and scoped `PushStyleColor` / `PushStyleVar` overrides are used only for local exceptions. That means the lowest-risk implementation is: add a theme enum plus three preset functions to `ImGuiLayer`, apply the chosen preset before `ImGui::NewFrame()`, and mirror the font menu with `View -> Interface Theme`.

Upstream Dear ImGui currently gives you exactly three stock styles out of the box: `Dark`, `Light`, and `Classic`. The official demo `ShowStyleSelector()` literally switches only between those three. Upstream also explicitly recommends using those stock style functions because the style system keeps evolving and custom styles can subtly break over time. For this repo, that means each prototype should start from `StyleColorsDark()` or `StyleColorsLight()` and then apply a small repo-local delta for warm palette, borders, spacing, and rounding. Do not author a giant full-color table from scratch and do not add a third-party skin framework.

The main repo-specific planning gap is persistence. The editor has a live font preset menu today, but the selected font is not persisted anywhere. Current persisted surfaces are `imgui.ini`, `editor_window.ini`, `editor_build.ini`, and layout presets; none store font/theme selection. Plan this phase as adding editor-local UI preferences explicitly. The cleanest low-risk option is a small gitignored `editor_ui.ini`-style file for `font_preset` and `theme_preset`, leaving `assets/project.cfg` reserved for project content state such as `last_scene`.

**Primary recommendation:** Add `ImGuiThemePreset` to `ImGuiLayer`, implement the three presets as small deltas over upstream `StyleColorsDark()` / `StyleColorsLight()`, expose `View -> Interface Theme`, and persist font/theme together in a small editor-local UI prefs file.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Dear ImGui | `v1.92.6-docking` | Global editor UI styling, docking, menus, style APIs | Already pinned in `CMakeLists.txt`; upstream docking branch is officially maintained and recommended. |
| Official Dear ImGui GLFW/OpenGL3 backends | `v1.92.6` | Backend integration for the current editor context | Already compiled from the same upstream tag; avoids any custom backend work for theme changes. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ImGui demo helpers (`ShowStyleEditor`, `ShowStyleSelector`) | `v1.92.6` | Temporary tuning aid while prototyping preset values | Use during implementation to inspect borders, rounding, and spacing; do not ship it as the permanent UX. |
| Repo-local editor UI prefs helper | new small helper | Persist selected theme/font across editor launches | Use because current editor has no persisted font/theme preference path. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Repo-local preset functions on stock styles | Third-party theme library or ImGui fork | Overkill for three presets and adds upgrade risk for little gain. |
| Dedicated editor-local UI prefs file | `assets/project.cfg` | Wrong scope: project content state vs user-local editor preference state. |
| Dedicated editor-local UI prefs file | Custom `imgui.ini` settings handler | More plumbing than this phase needs; lower-value than a simple key/value prefs file. |

**Installation:**
```bash
# No new dependencies are recommended for this phase.
# Keep using the repo-pinned Dear ImGui v1.92.6-docking stack.
```

**Version verification:** `CMakeLists.txt` pins `v1.92.6-docking`, and the upstream `v1.92.6` release and `v1.92.6-docking` tag were verified on 2026-04-04. Official release date: 2026-02-17.

## Architecture Patterns

### Recommended Project Structure
```text
src/
├── engine/ui/                 # ImGui global style/font preset application
│   ├── ImGuiLayer.h
│   └── ImGuiLayer.cpp
├── editor/core/               # Recommended home for editor-local UI prefs helper
│   └── EditorUiPreferences.h/.cpp
└── editor/ui/                 # Existing panel-level scoped style exceptions remain here

apps/
└── level_editor/main.cpp      # View menu, startup load, shutdown save, current selection state
```

### Pattern 1: Stock Style + Delta Presets
**What:** Each repo-local preset should call an upstream base style first, then override only the subset this phase cares about: window/panel backgrounds, headers, tabs, buttons, separators, borders, selection accents, rounding, spacing, and frame padding.
**When to use:** For all three presets.
**Example:**
```cpp
// Source: https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui_draw.cpp
void applyWarmStudioDark(ImGuiStyle& style) {
    ImGui::StyleColorsDark(&style);

    style.WindowRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.11f, 0.10f, 0.98f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.14f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.24f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.63f, 0.52f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.54f, 0.44f, 0.24f, 1.00f);
}
```

### Pattern 2: Pending Global Apply in `ImGuiLayer`
**What:** Mirror the existing pending font preset flow with `themePreset_`, `pendingThemePreset_`, `requestThemePreset()`, and `applyPendingThemePreset()` inside `ImGuiLayer`.
**When to use:** For live menu switching without restarting the editor.
**Example:**
```cpp
// Source: local repo pattern in src/engine/ui/ImGuiLayer.cpp + official guidance in imgui.h
enum class ImGuiThemePreset {
    WarmStudioDark,
    SpectrumInspiredDark,
    SoftLightTooling,
};

void ImGuiLayer::requestThemePreset(ImGuiThemePreset preset) {
    if (preset == themePreset_) {
        return;
    }
    pendingThemePreset_ = preset;
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    applyPendingThemePreset();
    applyPendingFontPreset();
    ImGui::NewFrame();
}
```

### Pattern 3: Editor-Only Persistence
**What:** Persist `font_preset` and `theme_preset` together in a small editor-local prefs file and load them before the first working editor frame.
**When to use:** On level editor startup/shutdown and when the user changes font or theme.
**Example:**
```cpp
// Source: repo-local persistence pattern; modeled after editor_build.ini usage
struct EditorUiPreferences {
    ImGuiFontPreset fontPreset = ImGuiFontPreset::SystemSans;
    ImGuiThemePreset themePreset = ImGuiThemePreset::WarmStudioDark;
};

EditorUiPreferences prefs = loadEditorUiPreferences("editor_ui.ini");
imgui.requestFontPreset(prefs.fontPreset);
imgui.requestThemePreset(prefs.themePreset);

// On selection change or shutdown:
saveEditorUiPreferences("editor_ui.ini", prefs);
```

### Anti-Patterns to Avoid
- **Full style tables from zero:** Upstream keeps adding style fields; starting from stock styles reduces maintenance and missing-field regressions.
- **Putting theme state into `assets/project.cfg`:** That file is project content state, not user-local editor UI state.
- **Encoding theme choice inside layout presets:** Layouts are about dock visibility and arrangement; theme is a separate concern.
- **Shipping `ShowStyleEditor()` as the final UX:** Good tuning aid, bad permanent theme system for this phase.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Theme architecture | Custom skinning framework, imported theme packs, or ImGui fork | Small `ImGuiThemePreset` enum + three preset functions | The phase only needs three reversible prototypes. |
| Theme authoring workflow | Permanent in-app theme editor | Temporary `ShowStyleEditor()` use during tuning, then bake values into code | The demo helper is already compiled and matches upstream style fields. |
| Persistence | Ad hoc storage in layout files or project content config | Small editor-local `editor_ui.ini` with `font_preset` + `theme_preset` | Keeps editor prefs local and explicit. |
| Panel-specific styling | Custom-drawn widgets or per-panel base themes | Global preset + existing scoped `PushStyleColor` / `PushStyleVar` overrides | The repo already uses scoped overrides successfully. |

**Key insight:** Dear ImGui’s style surface changes over time. The safest long-lived plan is not “invent a skin system”; it is “start from upstream stock styles, override the small subset that matters, and keep all editor-specific decisions in repo-local code.”

## Common Pitfalls

### Pitfall 1: Applying the Theme Too Late in the Frame
**What goes wrong:** Some windows use the old style while later windows use the new one, producing mixed styling in one frame.
**Why it happens:** `ImGuiStyle` is global for the context; mid-frame mutation is only safe for narrow scoped overrides.
**How to avoid:** Apply preset changes in `ImGuiLayer::beginFrame()` before `ImGui::NewFrame()`.
**Warning signs:** One-frame flashes, menu bar using one palette while popups use another.

### Pitfall 2: Assuming Font Persistence Already Exists
**What goes wrong:** The phase plan understates persistence work and forgets that font selection currently resets on restart.
**Why it happens:** The editor already has a live `Interface Font` menu, which looks like a persisted setting but is not.
**How to avoid:** Plan an explicit editor-local prefs file and decide whether font and theme should be saved together.
**Warning signs:** Theme persists in the plan, font does not; editor restarts back to `SystemSans` and stock dark.

### Pitfall 3: Making `Soft Light Tooling` Too Flat
**What goes wrong:** The light preset looks washed out and loses panel/input separation.
**Why it happens:** Upstream explicitly notes Light works best with borders and a thicker font.
**How to avoid:** Start from `StyleColorsLight()`, raise border visibility, keep stronger header/tab separation, and verify against existing font presets.
**Warning signs:** Inputs blend into panels, inactive tabs disappear, scrollbars feel invisible.

### Pitfall 4: Clobbering Existing Semantic Overrides
**What goes wrong:** Error/warning text, active-scene highlighting, selected tool buttons, and overlays lose their intended meaning.
**Why it happens:** The repo already uses scoped `PushStyleColor` and `PushStyleVar` exceptions in several editor surfaces.
**How to avoid:** Treat the global theme as the base layer only; keep local semantic overrides unless they become unreadable under the new preset.
**Warning signs:** Build errors lose red emphasis, active tool state disappears, help overlays become low-contrast.

### Pitfall 5: Choosing the Final Skin from Isolated Widgets
**What goes wrong:** A preset looks good in menus or buttons but fails in the actual editor workload.
**Why it happens:** Theme choice is aesthetic, but the editor’s real stress cases are docked windows, tabs, inspectors, build logs, and the viewport boundary.
**How to avoid:** Evaluate each preset in the real editor surfaces: docked layout, empty scene, busy scene, build output, popups, and help modal.
**Warning signs:** Final choice still feels wrong once multiple panels, tabs, and overlays are visible at once.

## Code Examples

Verified patterns from official sources and current repo structure:

### Base a Preset on Upstream `StyleColors*()`
```cpp
// Source: https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui_draw.cpp
void applySpectrumInspiredDark(ImGuiStyle& style) {
    ImGui::StyleColorsDark(&style);

    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.12f, 0.98f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.47f, 0.74f, 0.85f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.43f, 0.68f, 1.00f);
}
```

### Mirror the Existing Font Menu for Theme Selection
```cpp
// Source: local repo pattern in apps/level_editor/main.cpp
if (ImGui::BeginMenu("Interface Theme")) {
    static constexpr std::array<ImGuiThemePreset, 3> kThemePresets{
        ImGuiThemePreset::WarmStudioDark,
        ImGuiThemePreset::SpectrumInspiredDark,
        ImGuiThemePreset::SoftLightTooling,
    };

    for (ImGuiThemePreset preset : kThemePresets) {
        const bool selected = (editorThemePreset == preset);
        if (ImGui::MenuItem(imguiThemePresetLabel(preset), nullptr, selected)) {
            editorThemePreset = preset;
            imgui.requestThemePreset(preset);
        }
    }
    ImGui::EndMenu();
}
```

### Use `ShowStyleEditor()` Only as a Tuning Aid
```cpp
// Source: https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui_demo.cpp
bool showThemeLab = false;

if (showThemeLab) {
    ImGui::Begin("Theme Lab", &showThemeLab);
    ImGui::ShowStyleEditor();
    ImGui::End();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `StyleColorsClassic()` as the default feel | `StyleColorsDark()` is the default stock baseline | Dear ImGui changelog for 1.50 | Starting dark and warming it is the most natural path for this repo. |
| Hand-managed backend font texture rebuilds | Official OpenGL backend supports `ImGuiBackendFlags_RendererHasTextures` for dynamic font atlas updates | OpenGL backend change dated 2025-06-11; docs say this support will likely be required before June 2026 | Live font switching is lower risk than it used to be, and theme switching is simpler still. |
| Using untagged docking snapshots | Official tagged docking releases (`vX.Y.Z-docking`) | Docking wiki notes tagged docking releases since July 2023 | The repo can stay on an official docking tag and rely on current upstream docs. |

**Deprecated/outdated:**
- Full style-from-scratch authoring as the default strategy: upstream has warned for years that custom styles can subtly break as the style system evolves.
- Treating Light as a drop-in preset without extra borders/font consideration: upstream explicitly warns against that.

## Open Questions

1. **Should font persistence be folded into this phase together with theme persistence?**
   - What we know: the live font menu exists, but font choice is not saved today.
   - What's unclear: whether Phase 15 should persist both at once or only theme.
   - Recommendation: persist both together if a new `editor_ui.ini` is added; the incremental cost is small and avoids another prefs phase immediately after.

2. **Should the new default theme affect runtime debug overlays too, or editor only?**
   - What we know: `ImGuiLayer` is shared by the level editor and the runtime `RenderSystem`.
   - What's unclear: whether changing the `ImGuiLayer` default preset should intentionally change runtime debug overlay styling.
   - Recommendation: keep the persisted selector editor-only for now; make runtime default explicit and unchanged unless the implementation team decides the warm dark preset is also desirable there.

3. **Should a temporary “Theme Lab” window using `ShowStyleEditor()` be part of implementation work?**
   - What we know: `imgui_demo.cpp` is already compiled, so the API is available at zero dependency cost.
   - What's unclear: whether the planner should schedule it as a short-lived implementation aid.
   - Recommendation: yes, if used only for tuning and removed or hidden before phase close-out.

## Sources

### Primary (HIGH confidence)
- Local repo inspection:
  - `src/engine/ui/ImGuiLayer.h/.cpp` - current font preset flow, stock dark baseline, global apply point
  - `apps/level_editor/main.cpp` - existing `View -> Interface Font` menu, editor startup/shutdown persistence surfaces
  - `src/editor/core/LevelEditorCore.cpp` - layout preset persistence boundaries
  - `src/editor/build/EditorBuildSystem.h/.cpp` - existing gitignored key/value config pattern (`editor_build.ini`)
  - `.gitignore` - confirms `imgui.ini`, `editor_window.ini`, and `editor_build.ini` are editor-local state
- Official Dear ImGui release `v1.92.6` - https://github.com/ocornut/imgui/releases/tag/v1.92.6
- Official Dear ImGui `imgui.h` - https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui.h
- Official Dear ImGui `imgui_draw.cpp` - https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui_draw.cpp
- Official Dear ImGui `imgui_demo.cpp` - https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/imgui_demo.cpp
- Official Dear ImGui `docs/CHANGELOG.txt` - https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/docs/CHANGELOG.txt
- Official Dear ImGui `docs/BACKENDS.md` - https://raw.githubusercontent.com/ocornut/imgui/v1.92.6/docs/BACKENDS.md
- Official Docking wiki - https://github.com/ocornut/imgui/wiki/Docking

### Secondary (MEDIUM confidence)
- Official issue `#707` theme discussion history - https://github.com/ocornut/imgui/issues/707

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - verified against current upstream release/tag and local `CMakeLists.txt`.
- Architecture: HIGH - grounded in current repo files and official ImGui style APIs.
- Pitfalls: HIGH - derived from current repo behavior plus official upstream warnings about style evolution and Light theme requirements.

**Research date:** 2026-04-04
**Valid until:** 2026-05-04
