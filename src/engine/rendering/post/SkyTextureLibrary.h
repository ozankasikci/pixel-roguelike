#pragma once

#include "engine/rendering/assets/TextureCube.h"
#include "engine/rendering/assets/Texture2D.h"

#include <glad/gl.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class Shader;

class SkyTextureLibrary {
public:
    struct SkyReflectionSet {
        GLuint sourceCubemap = 0;
        GLuint prefilteredSpecularCubemap = 0;
        int specularMipCount = 1;
    };

    SkyTextureLibrary();
    ~SkyTextureLibrary();

    SkyTextureLibrary(const SkyTextureLibrary&) = delete;
    SkyTextureLibrary& operator=(const SkyTextureLibrary&) = delete;

    GLuint resolve(std::string_view path);
    GLuint resolveCube(const std::array<std::string, 6>& paths);
    const SkyReflectionSet& resolvePrefilteredCube(const std::array<std::string, 6>& paths);
    GLuint brdfLutTexture() const;
    void initReflectionResources();

private:
    void destroyReflectionResources();
    std::string cubemapKey(const std::array<std::string, 6>& paths) const;
    GLuint createPrefilteredCubemap(GLuint sourceCubemap, int baseResolution, int mipLevels);
    void ensureQuad();
    void ensureCube();

    Texture2D transparentFallback_{};
    TextureCube transparentCubeFallback_{};
    std::unordered_map<std::string, Texture2D> textures_{};
    std::unordered_map<std::string, TextureCube> cubeTextures_{};
    std::unordered_map<std::string, SkyReflectionSet> reflectionSets_{};
    std::unordered_set<std::string> failedPaths_{};
    std::unordered_set<std::string> failedCubeKeys_{};
    SkyReflectionSet transparentReflectionSet_{};
    std::unique_ptr<Shader> prefilterShader_;
    std::unique_ptr<Shader> brdfLutShader_;
    GLuint brdfLutTexture_ = 0;
    GLuint captureFbo_ = 0;
    GLuint captureRbo_ = 0;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    GLuint cubeVao_ = 0;
    GLuint cubeVbo_ = 0;
    bool reflectionResourcesInitialized_ = false;
};
