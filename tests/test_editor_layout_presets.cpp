#include "editor/EditorLayoutPreset.h"

#include <cassert>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;

    const fs::path tempDir = fs::temp_directory_path() / "gsd_editor_layout_test";
    fs::create_directories(tempDir);
    const fs::path presetPath = tempDir / "two_panel.layout";

    EditorLayoutPreset preset;
    preset.name = "Two Panel";
    preset.visibility.showOutliner = true;
    preset.visibility.showInspector = false;
    preset.visibility.showAssetBrowser = true;
    preset.visibility.showEnvironment = false;
    preset.visibility.showViewport = true;
    preset.imguiIni =
        "[Window][Viewport]\n"
        "Pos=100,120\n"
        "Size=900,700\n"
        "Collapsed=0\n"
        "[Docking][Data]\n"
        "DockSpace     ID=0x12345678 Window=0xABCDEF01 Pos=0,0 Size=1600,900 Split=X\n";

    saveEditorLayoutPreset(presetPath.string(), preset);
    const EditorLayoutPreset loaded = loadEditorLayoutPreset(presetPath.string());

    assert(loaded.name == "Two Panel");
    assert(loaded.visibility.showOutliner);
    assert(!loaded.visibility.showInspector);
    assert(loaded.visibility.showAssetBrowser);
    assert(!loaded.visibility.showEnvironment);
    assert(loaded.visibility.showViewport);
    assert(loaded.imguiIni == preset.imguiIni);

    fs::remove(presetPath);
    fs::remove(tempDir);
    return 0;
}
