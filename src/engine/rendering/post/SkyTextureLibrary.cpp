#include "engine/rendering/post/SkyTextureLibrary.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/core/Shader.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

namespace {

void createFullscreenQuad(GLuint& vao, GLuint& vbo) {
    float quadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void createCube(GLuint& vao, GLuint& vbo) {
    static constexpr float cubeVertices[] = {
        -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,

         1.0f,  1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,

        -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f,

        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f , 1.0f,  1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::array<glm::mat4, 6> captureViewProjections() {
    const glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    return {
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        captureProjection * glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    };
}

} // namespace

SkyTextureLibrary::SkyTextureLibrary() {
    std::vector<std::uint8_t> transparentPixel{0, 0, 0, 0};
    transparentFallback_.createRGBA8(1, 1, transparentPixel);
    std::array<std::vector<std::uint8_t>, 6> transparentFaces{};
    for (auto& face : transparentFaces) {
        face = transparentPixel;
    }
    transparentCubeFallback_.createRGBA8(1, 1, transparentFaces);
    transparentReflectionSet_.sourceCubemap = transparentCubeFallback_.id();
    transparentReflectionSet_.prefilteredSpecularCubemap = transparentCubeFallback_.id();
    transparentReflectionSet_.specularMipCount = 1;
}

SkyTextureLibrary::~SkyTextureLibrary() {
    destroyReflectionResources();
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
    const std::string resolved = resolveProjectPath(key);
    if (!texture.createRGBA8FromFile(resolved)) {
        failedPaths_.insert(key);
        spdlog::warn("SkyTextureLibrary: failed to load '{}'", key);
        return transparentFallback_.id();
    }

    auto [it, inserted] = textures_.emplace(key, std::move(texture));
    (void)inserted;
    return it->second.id();
}

std::string SkyTextureLibrary::cubemapKey(const std::array<std::string, 6>& paths) const {
    std::ostringstream keyStream;
    for (const std::string& path : paths) {
        keyStream << path << '\n';
    }
    return keyStream.str();
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

    const std::string key = cubemapKey(paths);

    auto existing = cubeTextures_.find(key);
    if (existing != cubeTextures_.end()) {
        return existing->second.id();
    }

    if (failedCubeKeys_.find(key) != failedCubeKeys_.end()) {
        return transparentCubeFallback_.id();
    }

    std::array<std::string, 6> resolvedPaths;
    for (std::size_t i = 0; i < 6; ++i) {
        resolvedPaths[i] = resolveProjectPath(paths[i]);
    }

    TextureCube texture;
    if (!texture.createRGBA8FromFiles(resolvedPaths)) {
        failedCubeKeys_.insert(key);
        spdlog::warn("SkyTextureLibrary: failed to load cubemap faces");
        return transparentCubeFallback_.id();
    }

    auto [it, inserted] = cubeTextures_.emplace(key, std::move(texture));
    (void)inserted;
    return it->second.id();
}

void SkyTextureLibrary::ensureQuad() {
    if (quadVao_ == 0) {
        createFullscreenQuad(quadVao_, quadVbo_);
    }
}

void SkyTextureLibrary::ensureCube() {
    if (cubeVao_ == 0) {
        createCube(cubeVao_, cubeVbo_);
    }
}

void SkyTextureLibrary::initReflectionResources() {
    if (reflectionResourcesInitialized_) {
        return;
    }

    prefilterShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/ibl_prefilter.vert",
        "assets/shaders/engine/ibl_prefilter.frag"
    );
    brdfLutShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/ibl_brdf_lut.vert",
        "assets/shaders/engine/ibl_brdf_lut.frag"
    );

    ensureQuad();
    ensureCube();

    glGenFramebuffers(1, &captureFbo_);
    glGenRenderbuffers(1, &captureRbo_);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 128, 128);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo_);

    glGenTextures(1, &brdfLutTexture_);
    glBindTexture(GL_TEXTURE_2D, brdfLutTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 256, 256, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLutTexture_, 0);
    glViewport(0, 0, 256, 256);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    brdfLutShader_->use();
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    reflectionResourcesInitialized_ = true;
}

GLuint SkyTextureLibrary::createPrefilteredCubemap(GLuint sourceCubemap, int baseResolution, int mipLevels) {
    GLuint prefilteredCubemap = 0;
    glGenTextures(1, &prefilteredCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilteredCubemap);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        for (int mip = 0; mip < mipLevels; ++mip) {
            const int mipSize = std::max(1, baseResolution >> mip);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                         mip,
                         GL_RGBA16F,
                         mipSize,
                         mipSize,
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         nullptr);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const auto viewProjections = captureViewProjections();
    prefilterShader_->use();
    prefilterShader_->setInt("uEnvironmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, sourceCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFbo_);
    glBindVertexArray(cubeVao_);
    glEnable(GL_DEPTH_TEST);

    for (int mip = 0; mip < mipLevels; ++mip) {
        const int mipSize = std::max(1, baseResolution >> mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);
        const float roughness = (mipLevels <= 1) ? 0.0f : static_cast<float>(mip) / static_cast<float>(mipLevels - 1);
        prefilterShader_->setFloat("uRoughness", roughness);

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            prefilterShader_->setMat4("uViewProjection", viewProjections[static_cast<std::size_t>(faceIndex)]);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                                   prefilteredCubemap,
                                   mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return prefilteredCubemap;
}

const SkyTextureLibrary::SkyReflectionSet& SkyTextureLibrary::resolvePrefilteredCube(const std::array<std::string, 6>& paths) {
    bool hasAllFaces = true;
    for (const std::string& path : paths) {
        if (path.empty()) {
            hasAllFaces = false;
            break;
        }
    }
    if (!hasAllFaces) {
        return transparentReflectionSet_;
    }

    initReflectionResources();
    const std::string key = cubemapKey(paths);
    auto existing = reflectionSets_.find(key);
    if (existing != reflectionSets_.end()) {
        return existing->second;
    }

    const GLuint sourceCubemap = resolveCube(paths);
    if (sourceCubemap == transparentCubeFallback_.id()) {
        return transparentReflectionSet_;
    }

    SkyReflectionSet set;
    set.sourceCubemap = sourceCubemap;
    set.specularMipCount = 6;
    set.prefilteredSpecularCubemap = createPrefilteredCubemap(sourceCubemap, 128, set.specularMipCount);
    auto [it, inserted] = reflectionSets_.emplace(key, set);
    (void)inserted;
    return it->second;
}

GLuint SkyTextureLibrary::brdfLutTexture() const {
    return brdfLutTexture_;
}

void SkyTextureLibrary::destroyReflectionResources() {
    for (auto& [key, set] : reflectionSets_) {
        (void)key;
        if (set.prefilteredSpecularCubemap != 0 && set.prefilteredSpecularCubemap != transparentCubeFallback_.id()) {
            glDeleteTextures(1, &set.prefilteredSpecularCubemap);
            set.prefilteredSpecularCubemap = 0;
        }
    }
    reflectionSets_.clear();

    if (brdfLutTexture_ != 0) {
        glDeleteTextures(1, &brdfLutTexture_);
        brdfLutTexture_ = 0;
    }
    if (captureRbo_ != 0) {
        glDeleteRenderbuffers(1, &captureRbo_);
        captureRbo_ = 0;
    }
    if (captureFbo_ != 0) {
        glDeleteFramebuffers(1, &captureFbo_);
        captureFbo_ = 0;
    }
    if (quadVao_ != 0) {
        glDeleteVertexArrays(1, &quadVao_);
        quadVao_ = 0;
    }
    if (quadVbo_ != 0) {
        glDeleteBuffers(1, &quadVbo_);
        quadVbo_ = 0;
    }
    if (cubeVao_ != 0) {
        glDeleteVertexArrays(1, &cubeVao_);
        cubeVao_ = 0;
    }
    if (cubeVbo_ != 0) {
        glDeleteBuffers(1, &cubeVbo_);
        cubeVbo_ = 0;
    }
    brdfLutShader_.reset();
    prefilterShader_.reset();
    reflectionResourcesInitialized_ = false;
}
