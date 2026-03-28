#pragma once

#include "engine/rendering/assets/TextureCube.h"
#include "engine/rendering/assets/Texture2D.h"

#include <glad/gl.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class SkyTextureLibrary {
public:
    SkyTextureLibrary();

    GLuint resolve(std::string_view path);
    GLuint resolveCube(const std::array<std::string, 6>& paths);

private:
    Texture2D transparentFallback_{};
    TextureCube transparentCubeFallback_{};
    std::unordered_map<std::string, Texture2D> textures_{};
    std::unordered_map<std::string, TextureCube> cubeTextures_{};
    std::unordered_set<std::string> failedPaths_{};
    std::unordered_set<std::string> failedCubeKeys_{};
};
