#pragma once

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

private:
    Texture2D transparentFallback_{};
    std::unordered_map<std::string, Texture2D> textures_{};
    std::unordered_set<std::string> failedPaths_{};
};
