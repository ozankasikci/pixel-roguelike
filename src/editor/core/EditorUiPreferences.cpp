#include <fstream>
#include <string_view>

#include "editor/core/EditorUiPreferences.h"

namespace {

constexpr std::string_view kFontPresetKey = "font_preset";
constexpr std::string_view kThemePresetKey = "theme_preset";

const char* fontPresetToken(ImGuiFontPreset preset) {
    switch (preset) {
    case ImGuiFontPreset::SystemSans:
        return "SystemSans";
    case ImGuiFontPreset::Verdana:
        return "Verdana";
    case ImGuiFontPreset::AvenirNext:
        return "AvenirNext";
    case ImGuiFontPreset::HelveticaNeue:
        return "HelveticaNeue";
    case ImGuiFontPreset::TrebuchetMS:
        return "TrebuchetMS";
    case ImGuiFontPreset::InterUnity:
        return "InterUnity";
    case ImGuiFontPreset::RobotoUnreal:
        return "RobotoUnreal";
    case ImGuiFontPreset::JetBrainsMonoGodot:
        return "JetBrainsMonoGodot";
    }

    return "SystemSans";
}

ImGuiFontPreset parseFontPreset(std::string_view token) {
    if (token == "SystemSans") {
        return ImGuiFontPreset::SystemSans;
    }
    if (token == "Verdana") {
        return ImGuiFontPreset::Verdana;
    }
    if (token == "AvenirNext") {
        return ImGuiFontPreset::AvenirNext;
    }
    if (token == "HelveticaNeue") {
        return ImGuiFontPreset::HelveticaNeue;
    }
    if (token == "TrebuchetMS") {
        return ImGuiFontPreset::TrebuchetMS;
    }
    if (token == "InterUnity") {
        return ImGuiFontPreset::InterUnity;
    }
    if (token == "RobotoUnreal") {
        return ImGuiFontPreset::RobotoUnreal;
    }
    if (token == "JetBrainsMonoGodot") {
        return ImGuiFontPreset::JetBrainsMonoGodot;
    }
    return ImGuiFontPreset::SystemSans;
}

const char* themePresetToken(ImGuiThemePreset preset) {
    switch (preset) {
    case ImGuiThemePreset::WarmStudioDark:
        return "WarmStudioDark";
    case ImGuiThemePreset::SpectrumInspiredDark:
        return "SpectrumInspiredDark";
    case ImGuiThemePreset::SoftLightTooling:
        return "SoftLightTooling";
    }

    return "WarmStudioDark";
}

ImGuiThemePreset parseThemePreset(std::string_view token) {
    if (token == "WarmStudioDark") {
        return ImGuiThemePreset::WarmStudioDark;
    }
    if (token == "SpectrumInspiredDark") {
        return ImGuiThemePreset::SpectrumInspiredDark;
    }
    if (token == "SoftLightTooling") {
        return ImGuiThemePreset::SoftLightTooling;
    }
    return ImGuiThemePreset::WarmStudioDark;
}

void trimLineEnding(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

} // namespace

EditorUiPreferences loadEditorUiPreferences(const std::string& path) {
    EditorUiPreferences prefs;

    std::ifstream file(path);
    if (!file.is_open()) {
        return prefs;
    }

    std::string line;
    while (std::getline(file, line)) {
        trimLineEnding(line);
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string_view key(line.data(), separator);
        const std::string_view value(line.data() + separator + 1, line.size() - separator - 1);
        if (key == kFontPresetKey) {
            prefs.fontPreset = parseFontPreset(value);
        } else if (key == kThemePresetKey) {
            prefs.themePreset = parseThemePreset(value);
        }
    }

    return prefs;
}

void saveEditorUiPreferences(const EditorUiPreferences& prefs, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return;
    }

    file << "font_preset=" << fontPresetToken(prefs.fontPreset) << "\n";
    file << "theme_preset=" << themePresetToken(prefs.themePreset) << "\n";
}
