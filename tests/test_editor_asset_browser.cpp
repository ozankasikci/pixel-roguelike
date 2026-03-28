#include "editor/assets/EditorAssetBrowser.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    namespace fs = std::filesystem;

    const fs::path tempRoot = fs::temp_directory_path() / "gsd_editor_asset_browser_test";
    fs::remove_all(tempRoot);
    fs::create_directories(tempRoot / "meshes");
    fs::create_directories(tempRoot / "defs" / "materials");
    fs::create_directories(tempRoot / "scenes");

    {
        std::ofstream meshFile(tempRoot / "meshes" / "pillar.glb");
        meshFile << "mesh";
    }
    {
        std::ofstream materialFile(tempRoot / "defs" / "materials" / "stone.material");
        materialFile << "id stone_default\n";
    }
    {
        std::ofstream sceneFile(tempRoot / "scenes" / "cloister.scene");
        sceneFile << "environment_profile cloister_daylight\n";
    }
    {
        std::ofstream hiddenFile(tempRoot / ".DS_Store");
        hiddenFile << "junk";
    }

    const auto tree = buildEditorAssetBrowserTree(tempRoot, tempRoot.parent_path());
    assert(tree.size() == 3);
    assert(tree[0].directory);
    assert(tree[1].directory);
    assert(tree[2].directory);

    const auto meshNode = tree[1].children.front();
    assert(meshNode.kind == EditorAssetBrowserKind::Mesh);
    assert(editorMeshIdForAssetPath(meshNode.absolutePath) == "pillar");

    const auto materialNode = tree[0].children.front().children.front();
    assert(materialNode.kind == EditorAssetBrowserKind::Material);
    assert(readEditorAssetDeclaredId(materialNode.absolutePath).value() == "stone_default");

    const auto sceneNode = tree[2].children.front();
    assert(sceneNode.kind == EditorAssetBrowserKind::Scene);

    fs::remove_all(tempRoot);
    return 0;
}
