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
    assert(missingPrefs.themePreset == ImGuiThemePreset::WarmStudioDark);

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
        std::ofstream unknownFile(prefsPath);
        unknownFile << "font_preset=Nope\n";
        unknownFile << "theme_preset=Nope\n";
        unknownFile << "unused_key=Ignored\n";
    }

    const EditorUiPreferences unknownPrefs = loadEditorUiPreferences(prefsPath.string());
    assert(unknownPrefs.fontPreset == ImGuiFontPreset::SystemSans);
    assert(unknownPrefs.themePreset == ImGuiThemePreset::WarmStudioDark);

    fs::remove(prefsPath);
    fs::remove(tempDir);
    return 0;
}
