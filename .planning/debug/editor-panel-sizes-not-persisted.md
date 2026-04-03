---
status: awaiting_human_verify
trigger: "editor-panel-sizes-not-persisted"
created: 2026-04-01T00:00:00Z
updated: 2026-04-01T00:00:00Z
---

## Current Focus
<!-- OVERWRITE on each update - reflects NOW -->

hypothesis: CONFIRMED — startup unconditionally calls loadLayoutPresetIntoUi("default") which calls LoadIniSettingsFromMemory, overwriting imgui.ini auto-load data (which contains user's panel sizes) with the stored default.layout ini data
test: N/A — root cause confirmed
expecting: N/A
next_action: Fix startup to only apply layout visibility, not load ini data on startup (leave ini to ImGui auto-load)

## Evidence
<!-- APPEND only - facts discovered -->

- timestamp: 2026-04-01T00:00:00Z
  checked: imgui.ini existence and content
  found: imgui.ini exists at project root with user-resized dock node sizes (e.g. Outliner SizeRef=326), different from default.layout (SizeRef=559)
  implication: ImGui auto-loads imgui.ini on first NewFrame() during startup progress frames — user sizes ARE saved correctly

- timestamp: 2026-04-01T00:00:00Z
  checked: apps/level_editor/main.cpp lines 353-362
  found: On startup, loadLayoutPresetIntoUi(ui, "default") is always called because "default" exists in layoutPresetNames
  implication: This calls ImGui::LoadIniSettingsFromMemory() with default.layout's ini data, OVERWRITING the user sizes that ImGui just loaded from imgui.ini

- timestamp: 2026-04-01T00:00:00Z
  checked: ImGuiLayer::init() in src/engine/ui/ImGuiLayer.cpp
  found: io.IniFilename is never set to nullptr — ImGui uses its default "imgui.ini" auto-save/load behavior
  implication: ImGui correctly auto-saves panel sizes at shutdown and auto-loads at startup, but loadLayoutPresetIntoUi overwrites this on each startup

- timestamp: 2026-04-01T00:00:00Z
  checked: LevelEditorCore.cpp loadLayoutPresetIntoUi and saveLayoutPresetFromUi
  found: saveLayoutPresetFromUi calls ImGui::SaveIniSettingsToMemory() (correct), loadLayoutPresetIntoUi calls ImGui::LoadIniSettingsFromMemory() (correct for user-triggered layout switch, wrong for startup)
  implication: Layout switching works correctly; the bug is only the startup call that unconditionally loads the default.layout ini

## Symptoms
<!-- Written during gathering, then IMMUTABLE -->

expected: When the user resizes editor panels (inspector, outliner, viewport, etc.) and closes/reopens the editor, the panel sizes should be restored to what they were.
actual: Panel sizes reset to default values every time the editor is reopened.
errors: No error messages — functional issue.
reproduction: Open the level editor, resize some panels, close the editor, reopen it — panels are back to default sizes.
started: Ongoing issue.

## Eliminated
<!-- APPEND only - prevents re-investigating -->

## Evidence
<!-- APPEND only - facts discovered -->

## Resolution
<!-- OVERWRITE as understanding evolves -->

root_cause: On startup, the editor always called loadLayoutPresetIntoUi("default") which calls ImGui::LoadIniSettingsFromMemory() with the stored default.layout ini data. This overwrites the dock node sizes that ImGui auto-loaded from imgui.ini (which contains the user's actual panel sizes) during the startup progress frames. Since this happened every startup, user panel size customizations were discarded each time.
fix: Two changes in apps/level_editor/main.cpp:
  1. Startup now applies only layout visibility (show/hide panels) from the preset, not the imgui ini data. ImGui's built-in auto-load from imgui.ini handles dock sizes.
  2. Added explicit ImGui::SaveIniSettingsToDisk() call before shutdown so panel sizes are always captured even if the user closes within the 5-second periodic auto-save window.
verification: awaiting user confirmation
files_changed: [apps/level_editor/main.cpp]
