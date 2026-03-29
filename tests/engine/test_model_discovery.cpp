#include "engine/rendering/assets/ModelLoader.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

const DiscoveredModelAsset* findAsset(const std::vector<DiscoveredModelAsset>& assets, const std::string& meshId) {
    for (const auto& asset : assets) {
        if (asset.meshId == meshId) {
            return &asset;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path tempRoot = test_support::resetTempDirectory("gsd_model_discovery_test");
    fs::create_directories(tempRoot / "nested");
    fs::create_directories(tempRoot / ".hidden");

    {
        std::ofstream modelFile(tempRoot / "pillar.glb");
        modelFile << "mesh";
    }
    {
        std::ofstream modelFile(tempRoot / "nested" / "hero.FBX");
        modelFile << "mesh";
    }
    {
        std::ofstream otherFile(tempRoot / "nested" / "notes.txt");
        otherFile << "ignore";
    }
    {
        std::ofstream hiddenModelFile(tempRoot / ".hidden" / "skip.fbx");
        hiddenModelFile << "hidden";
    }

    assert(ModelLoader::supportsExtension(".fbx"));
    assert(ModelLoader::supportsPath(tempRoot / "nested" / "hero.FBX"));
    assert(!ModelLoader::supportsPath(tempRoot / "nested" / "notes.txt"));

    const auto assets = ModelLoader::discoverProjectAssets(tempRoot, tempRoot);
    assert(assets.size() == 2);

    const DiscoveredModelAsset* pillar = findAsset(assets, "pillar");
    assert(pillar != nullptr);
    assert(pillar->relativePath == "pillar.glb");

    const DiscoveredModelAsset* hero = findAsset(assets, "hero");
    assert(hero != nullptr);
    assert(hero->relativePath == "nested/hero.FBX");

    fs::remove_all(tempRoot);
    return 0;
}
