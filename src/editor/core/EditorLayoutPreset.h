#pragma once

#include <string>
#include <vector>

struct EditorLayoutVisibility {
    bool showOutliner = true;
    bool showInspector = true;
    bool showAssetBrowser = true;
    bool showEnvironment = true;
    bool showViewport = true;
};

struct EditorLayoutPreset {
    std::string name;
    EditorLayoutVisibility visibility;
    std::string imguiIni;
};

std::string editorLayoutPresetDirectory();
std::string editorLayoutPresetPath(const std::string& name);
std::vector<std::string> listEditorLayoutPresetNames();

EditorLayoutPreset loadEditorLayoutPreset(const std::string& path);
void saveEditorLayoutPreset(const std::string& path, const EditorLayoutPreset& preset);
