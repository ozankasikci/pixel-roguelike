#include "editor/assets/EditorAssetBrowser.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

const EditorAssetBrowserNode* findNodeByName(const std::vector<EditorAssetBrowserNode>& nodes,
                                             const std::string& name) {
    for (const auto& node : nodes) {
        if (node.name == name) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path tempProject = test_support::resetTempDirectory("gsd_editor_asset_browser_test");
    const fs::path assetsRoot = tempProject / "assets";
    fs::create_directories(assetsRoot / "meshes" / "custom");
    fs::create_directories(assetsRoot / "defs" / "materials");
    fs::create_directories(assetsRoot / "scenes");
    fs::create_directories(tempProject / "imports");

    {
        std::ofstream meshFile(assetsRoot / "meshes" / "pillar.glb");
        meshFile << "mesh";
    }
    {
        std::ofstream meshFile(assetsRoot / "meshes" / "hero.FBX");
        meshFile << "mesh";
    }
    {
        std::ofstream materialFile(assetsRoot / "defs" / "materials" / "stone.material");
        materialFile << "id stone_default\n";
    }
    {
        std::ofstream sceneFile(assetsRoot / "scenes" / "cloister.scene");
        sceneFile << "environment_profile cloister_daylight\n";
    }
    {
        std::ofstream hiddenFile(assetsRoot / ".DS_Store");
        hiddenFile << "junk";
    }
    const fs::path externalMeshPath = tempProject / "imports" / "sarcophagus.fbx";
    {
        std::ofstream meshFile(externalMeshPath);
        meshFile << "mesh";
    }

    const auto tree = buildEditorAssetBrowserTree(assetsRoot, tempProject);
    assert(tree.size() == 3);
    assert(tree[0].directory);
    assert(tree[1].directory);
    assert(tree[2].directory);

    const EditorAssetBrowserNode* glbNode = findNodeByName(tree[1].children, "pillar.glb");
    assert(glbNode != nullptr);
    assert(glbNode->kind == EditorAssetBrowserKind::Mesh);
    assert(editorMeshIdForAssetPath(glbNode->absolutePath) == "pillar");

    const EditorAssetBrowserNode* fbxNode = findNodeByName(tree[1].children, "hero.FBX");
    assert(fbxNode != nullptr);
    assert(fbxNode->kind == EditorAssetBrowserKind::Mesh);
    assert(editorMeshIdForAssetPath(fbxNode->absolutePath) == "hero");

    const auto materialNode = tree[0].children.front().children.front();
    assert(materialNode.kind == EditorAssetBrowserKind::Material);
    assert(readEditorAssetDeclaredId(materialNode.absolutePath).value() == "stone_default");

    const auto sceneNode = tree[2].children.front();
    assert(sceneNode.kind == EditorAssetBrowserKind::Scene);

    assert(editorAssetImportDirectory(assetsRoot, "assets/meshes/custom", true) == assetsRoot / "meshes" / "custom");
    assert(editorAssetImportDirectory(assetsRoot, "assets/scenes/cloister.scene", false) == assetsRoot / "meshes");

    const auto imported = importEditorExternalAssets({externalMeshPath}, assetsRoot, "assets/meshes/custom", true);
    assert(imported.size() == 1);
    assert(imported.front().kind == EditorAssetBrowserKind::Mesh);
    assert(imported.front().meshId == "sarcophagus");
    assert(imported.front().relativePath == "assets/meshes/custom/sarcophagus.fbx");
    assert(fs::exists(tempProject / imported.front().relativePath));

    const auto importedDuplicate = importEditorExternalAssets({externalMeshPath}, assetsRoot, "assets/meshes/custom", true);
    assert(importedDuplicate.size() == 1);
    assert(importedDuplicate.front().relativePath == "assets/meshes/custom/sarcophagus_1.fbx");
    assert(fs::exists(tempProject / importedDuplicate.front().relativePath));

    fs::remove_all(tempProject);
    return 0;
}
