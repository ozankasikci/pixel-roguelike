#pragma once

#include <string>

#include "engine/ui/ImGuiLayer.h"

struct EditorUiPreferences {
    ImGuiFontPreset fontPreset = ImGuiFontPreset::SystemSans;
    ImGuiThemePreset themePreset = ImGuiThemePreset::GraphiteDense;
};

EditorUiPreferences loadEditorUiPreferences(const std::string& path);
void saveEditorUiPreferences(const EditorUiPreferences& prefs, const std::string& path);
