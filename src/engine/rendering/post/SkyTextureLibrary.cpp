#include "engine/rendering/post/SkyTextureLibrary.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>
#include <sstream>
#include <vector>

SkyTextureLibrary::SkyTextureLibrary() {
    std::vector<std::uint8_t> transparentPixel{0, 0, 0, 0};
    transparentFallback_.createRGBA8(1, 1, transparentPixel);
    std::array<std::vector<std::uint8_t>, 6> transparentFaces{};
    for (auto& face : transparentFaces) {
        face = transparentPixel;
    }
    transparentCubeFallback_.createRGBA8(1, 1, transparentFaces);
}

GLuint SkyTextureLibrary::resolve(std::string_view path) {
    if (path.empty()) {
        return transparentFallback_.id();
    }

    const std::string key(path);
    auto existing = textures_.find(key);
    if (existing != textures_.end()) {
        return existing->second.id();
    }

    if (failedPaths_.find(key) != failedPaths_.end()) {
        return transparentFallback_.id();
    }

    Texture2D texture;
    if (!texture.createRGBA8FromFile(key)) {
        failedPaths_.insert(key);
        spdlog::warn("SkyTextureLibrary: failed to load '{}'", key);
        return transparentFallback_.id();
    }

    auto [it, inserted] = textures_.emplace(key, std::move(texture));
    (void)inserted;
    return it->second.id();
}

GLuint SkyTextureLibrary::resolveCube(const std::array<std::string, 6>& paths) {
    bool hasAllFaces = true;
    for (const std::string& path : paths) {
        if (path.empty()) {
            hasAllFaces = false;
            break;
        }
    }
    if (!hasAllFaces) {
        return transparentCubeFallback_.id();
    }

    std::ostringstream keyStream;
    for (const std::string& path : paths) {
        keyStream << path << '\n';
    }
    const std::string key = keyStream.str();

    auto existing = cubeTextures_.find(key);
    if (existing != cubeTextures_.end()) {
        return existing->second.id();
    }

    if (failedCubeKeys_.find(key) != failedCubeKeys_.end()) {
        return transparentCubeFallback_.id();
    }

    TextureCube texture;
    if (!texture.createRGBA8FromFiles(paths)) {
        failedCubeKeys_.insert(key);
        spdlog::warn("SkyTextureLibrary: failed to load cubemap faces");
        return transparentCubeFallback_.id();
    }

    auto [it, inserted] = cubeTextures_.emplace(key, std::move(texture));
    (void)inserted;
    return it->second.id();
}
