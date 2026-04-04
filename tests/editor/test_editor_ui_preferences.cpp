#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "common/TestSupport.h"
#include "editor/core/EditorUiPreferences.h"

int main() {
    namespace fs = std::filesystem;

    const fs::path tempDir = test_support::resetTempDirectory("gsd_editor_ui_preferences_test");
    const fs::path prefsPath = tempDir / "editor_ui.ini";

    const EditorUiPreferences missingPrefs = loadEditorUiPreferences(prefsPath.string());
    assert(missingPrefs.fontPreset == ImGuiFontPreset::SystemSans);
    assert(missingPrefs.themePreset == ImGuiThemePreset::GraphiteDense);

    saveEditorUiPreferences(EditorUiPreferences{
                                .fontPreset = ImGuiFontPreset::RobotoUnreal,
                                .themePreset = ImGuiThemePreset::SpectrumInspiredDark,
                            },
                            prefsPath.string());

    std::ifstream roundtripFile(prefsPath);
    const std::string roundtripContents((std::istreambuf_iterator<char>(roundtripFile)),
                                        std::istreambuf_iterator<char>());
    assert(roundtripContents == "font_preset=RobotoUnreal\ntheme_preset=SpectrumInspiredDark\n");

    const EditorUiPreferences roundtripPrefs = loadEditorUiPreferences(prefsPath.string());
    assert(roundtripPrefs.fontPreset == ImGuiFontPreset::RobotoUnreal);
    assert(roundtripPrefs.themePreset == ImGuiThemePreset::SpectrumInspiredDark);

    {
        std::ofstream graphiteFile(prefsPath);
        graphiteFile << "font_preset=InterUnity\n";
        graphiteFile << "theme_preset=GraphiteDark\n";
    }

    const EditorUiPreferences graphitePrefs = loadEditorUiPreferences(prefsPath.string());
    assert(graphitePrefs.fontPreset == ImGuiFontPreset::InterUnity);
    assert(graphitePrefs.themePreset == ImGuiThemePreset::GraphiteDark);

    {
        std::ofstream denseFile(prefsPath);
        denseFile << "font_preset=SystemSans\n";
        denseFile << "theme_preset=GraphiteDense\n";
    }

    const EditorUiPreferences densePrefs = loadEditorUiPreferences(prefsPath.string());
    assert(densePrefs.fontPreset == ImGuiFontPreset::SystemSans);
    assert(densePrefs.themePreset == ImGuiThemePreset::GraphiteDense);

    {
        std::ofstream compactFile(prefsPath);
        compactFile << "font_preset=Verdana\n";
        compactFile << "theme_preset=SpectrumCompact\n";
    }

    const EditorUiPreferences compactPrefs = loadEditorUiPreferences(prefsPath.string());
    assert(compactPrefs.fontPreset == ImGuiFontPreset::Verdana);
    assert(compactPrefs.themePreset == ImGuiThemePreset::SpectrumCompact);

    {
        std::ofstream unknownFile(prefsPath);
        unknownFile << "font_preset=Nope\n";
        unknownFile << "theme_preset=Nope\n";
        unknownFile << "unused_key=Ignored\n";
    }

    const EditorUiPreferences unknownPrefs = loadEditorUiPreferences(prefsPath.string());
    assert(unknownPrefs.fontPreset == ImGuiFontPreset::SystemSans);
    assert(unknownPrefs.themePreset == ImGuiThemePreset::GraphiteDense);

    fs::remove(prefsPath);
    fs::remove(tempDir);
    return 0;
}
