#include "editor/EditorLayoutPreset.h"

#include "engine/core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

constexpr const char* kImguiIniBegin = "imgui_ini_begin";
constexpr const char* kImguiIniEnd = "imgui_ini_end";

std::filesystem::path projectRootPath() {
    const std::filesystem::path assetsPath(resolveProjectPath("assets"));
    if (assetsPath.filename() == "assets") {
        return assetsPath.parent_path();
    }
    return std::filesystem::current_path();
}

std::string trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

bool parseBool(const std::string& value) {
    return value == "1" || value == "true" || value == "TRUE";
}

std::string sanitizeLayoutName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (result.empty() || result.back() == '_') {
                continue;
            }
            result.push_back('_');
        }
    }

    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result.empty() ? "layout" : result;
}

} // namespace

std::string editorLayoutPresetDirectory() {
    return (projectRootPath() / "editor_layouts").string();
}

std::string editorLayoutPresetPath(const std::string& name) {
    const std::filesystem::path directory(editorLayoutPresetDirectory());
    return (directory / (sanitizeLayoutName(name) + ".layout")).string();
}

std::vector<std::string> listEditorLayoutPresetNames() {
    namespace fs = std::filesystem;

    std::vector<std::string> names;
    const fs::path directory(editorLayoutPresetDirectory());
    if (!fs::exists(directory)) {
        return names;
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".layout") {
            continue;
        }
        names.push_back(entry.path().stem().string());
    }

    std::sort(names.begin(), names.end());
    return names;
}

EditorLayoutPreset loadEditorLayoutPreset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open editor layout preset: " + path);
    }

    EditorLayoutPreset preset;
    preset.name = std::filesystem::path(path).stem().string();

    bool readingIni = false;
    std::ostringstream iniStream;
    std::string line;
    while (std::getline(file, line)) {
        if (readingIni) {
            if (line == kImguiIniEnd) {
                readingIni = false;
                continue;
            }
            iniStream << line << '\n';
            continue;
        }

        if (line == kImguiIniBegin) {
            readingIni = true;
            continue;
        }

        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        const std::size_t separator = trimmed.find(' ');
        const std::string key = separator == std::string::npos ? trimmed : trimmed.substr(0, separator);
        const std::string value = separator == std::string::npos ? "" : trim(trimmed.substr(separator + 1));

        if (key == "name") {
            preset.name = value;
        } else if (key == "show_outliner") {
            preset.visibility.showOutliner = parseBool(value);
        } else if (key == "show_inspector") {
            preset.visibility.showInspector = parseBool(value);
        } else if (key == "show_asset_browser") {
            preset.visibility.showAssetBrowser = parseBool(value);
        } else if (key == "show_environment") {
            preset.visibility.showEnvironment = parseBool(value);
        } else if (key == "show_viewport") {
            preset.visibility.showViewport = parseBool(value);
        }
    }

    preset.imguiIni = iniStream.str();
    return preset;
}

void saveEditorLayoutPreset(const std::string& path, const EditorLayoutPreset& preset) {
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to save editor layout preset: " + path);
    }

    file << "name " << preset.name << '\n';
    file << "show_outliner " << (preset.visibility.showOutliner ? 1 : 0) << '\n';
    file << "show_inspector " << (preset.visibility.showInspector ? 1 : 0) << '\n';
    file << "show_asset_browser " << (preset.visibility.showAssetBrowser ? 1 : 0) << '\n';
    file << "show_environment " << (preset.visibility.showEnvironment ? 1 : 0) << '\n';
    file << "show_viewport " << (preset.visibility.showViewport ? 1 : 0) << '\n';
    file << kImguiIniBegin << '\n';
    file << preset.imguiIni;
    if (!preset.imguiIni.empty() && preset.imguiIni.back() != '\n') {
        file << '\n';
    }
    file << kImguiIniEnd << '\n';
}
