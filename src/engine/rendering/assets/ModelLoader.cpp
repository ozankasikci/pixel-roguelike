#include "engine/rendering/assets/ModelLoader.h"

#include "engine/rendering/assets/AssimpLoader.h"
#include "engine/rendering/assets/GltfLoader.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace {

std::string lowercased(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isHiddenPath(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && name[0] == '.';
}

} // namespace

bool ModelLoader::supportsExtension(const std::string& extension) {
    const std::string normalized = lowercased(extension);
    return normalized == ".glb" || normalized == ".gltf" || normalized == ".fbx";
}

bool ModelLoader::supportsPath(const std::filesystem::path& path) {
    return supportsExtension(path.extension().string());
}

std::string ModelLoader::meshIdForPath(const std::filesystem::path& path) {
    return path.stem().string();
}

std::vector<DiscoveredModelAsset> ModelLoader::discoverProjectAssets(const std::filesystem::path& rootPath,
                                                                     const std::filesystem::path& relativeBase) {
    std::vector<DiscoveredModelAsset> assets;
    if (!std::filesystem::exists(rootPath)) {
        return assets;
    }

    std::error_code errorCode;
    for (std::filesystem::recursive_directory_iterator it(
             rootPath,
             std::filesystem::directory_options::skip_permission_denied,
             errorCode);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(errorCode)) {
        if (errorCode) {
            spdlog::warn("ModelLoader: skipping asset scan error under '{}': {}",
                         rootPath.string(),
                         errorCode.message());
            errorCode.clear();
            continue;
        }

        const auto& entry = *it;
        if (isHiddenPath(entry.path())) {
            if (entry.is_directory()) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!supportsPath(entry.path())) {
            continue;
        }

        std::filesystem::path relativePath;
        std::error_code relativeError;
        relativePath = std::filesystem::relative(entry.path(), relativeBase, relativeError);
        if (relativeError) {
            relativePath = entry.path().lexically_normal();
        }

        assets.push_back(DiscoveredModelAsset{
            .meshId = meshIdForPath(entry.path()),
            .relativePath = relativePath.generic_string(),
            .absolutePath = entry.path().lexically_normal(),
        });
    }

    std::sort(assets.begin(), assets.end(), [](const DiscoveredModelAsset& lhs, const DiscoveredModelAsset& rhs) {
        return lhs.relativePath < rhs.relativePath;
    });
    return assets;
}

std::unique_ptr<Mesh> ModelLoader::load(const std::string& filepath) {
    RawMeshData raw = loadRaw(filepath);
    if (raw.positions.empty() || raw.indices.empty()) {
        return nullptr;
    }
    return std::make_unique<Mesh>(raw.positions, raw.normals, raw.uvs, raw.tangents, raw.indices);
}

RawMeshData ModelLoader::loadRaw(const std::string& filepath) {
    const std::string extension = lowercased(std::filesystem::path(filepath).extension().string());
    if (extension == ".glb" || extension == ".gltf") {
        return GltfLoader::loadRaw(filepath);
    }
    if (extension == ".fbx") {
        return AssimpLoader::loadRaw(filepath);
    }

    spdlog::error("ModelLoader::loadRaw: unsupported model format '{}'", filepath);
    return {};
}
