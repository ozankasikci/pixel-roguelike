#include "engine/rendering/post/SkyTextureLibrary.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <vector>

SkyTextureLibrary::SkyTextureLibrary() {
    std::vector<std::uint8_t> transparentPixel{0, 0, 0, 0};
    transparentFallback_.createRGBA8(1, 1, transparentPixel);
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
