#include "engine/rendering/lighting/CascadedShadowMap.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cfloat>
#include <stdexcept>

CascadedShadowMap::~CascadedShadowMap() {
    destroy();
}

void CascadedShadowMap::create(int resolution) {
    resolution_ = resolution;

    // Create GL_TEXTURE_2D_ARRAY with one layer per cascade
    glGenTextures(1, &depthArray_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY,
                 0,
                 GL_DEPTH_COMPONENT32F,
                 resolution, resolution, kCascadeCount,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Enable hardware depth comparison (GL_COMPARE_REF_TO_TEXTURE)
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Create FBO and attach the entire array (all layers) via glFramebufferTexture
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    // Use glFramebufferTexture (not glFramebufferTexture2D) to attach all layers at once
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("CSM framebuffer is not complete: {:#x}", static_cast<unsigned>(status));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("CSM framebuffer creation failed");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowMap::destroy() {
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (depthArray_ != 0) {
        glDeleteTextures(1, &depthArray_);
        depthArray_ = 0;
    }
    resolution_ = 0;
}

void CascadedShadowMap::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, resolution_, resolution_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void CascadedShadowMap::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowMap::computeCascades(const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix,
                                         const glm::vec3& lightDirection,
                                         float nearPlane, float farPlane,
                                         float lambda) {
    // PSSM cascade split blending.
    // lambda=0 produces uniform linear splits; lambda=1 produces fully logarithmic splits.
    // Blended formula: split_i = lerp(linear_i, log_i, lambda)
    //   linear_i = near + (far - near) * (i+1) / kCascadeCount
    //   log_i    = near * pow(far/near, (i+1) / kCascadeCount)
    const float clampedLambda = std::clamp(lambda, 0.0f, 1.0f);
    float cascadeSplits[kCascadeCount];
    for (int i = 0; i < kCascadeCount; ++i) {
        const float fraction = static_cast<float>(i + 1) / static_cast<float>(kCascadeCount);
        const float linearSplit = nearPlane + (farPlane - nearPlane) * fraction;
        const float logSplit = nearPlane * std::pow(farPlane / nearPlane, fraction);
        cascadeSplits[i] = clampedLambda * logSplit + (1.0f - clampedLambda) * linearSplit;
    }
    // Always end at farPlane
    cascadeSplits[kCascadeCount - 1] = farPlane;

    for (int i = 0; i < kCascadeCount; ++i) {
        splitDistances_[i] = cascadeSplits[i];
    }

    float prevSplit = nearPlane;
    for (int i = 0; i < kCascadeCount; ++i) {
        float currentSplit = cascadeSplits[i];

        // Build a sub-frustum projection for [prevSplit, currentSplit]
        const glm::mat4 subProj = glm::perspective(
            glm::radians(2.0f * glm::degrees(std::atan(1.0f / projectionMatrix[1][1]))),
            projectionMatrix[1][1] / projectionMatrix[0][0],
            prevSplit,
            currentSplit
        );

        const std::vector<glm::vec4> corners = getFrustumCornersWorldSpace(subProj, viewMatrix);
        lightSpaceMatrices_[i] = buildCascadeMatrix(lightDirection, corners);
        prevSplit = currentSplit;
    }
}

std::vector<glm::vec4> CascadedShadowMap::getFrustumCornersWorldSpace(const glm::mat4& proj,
                                                                        const glm::mat4& view) const {
    const glm::mat4 inv = glm::inverse(proj * view);
    std::vector<glm::vec4> corners;
    corners.reserve(8);
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f,
                                               2.0f * y - 1.0f,
                                               2.0f * z - 1.0f, 1.0f);
                corners.push_back(pt / pt.w);
            }
        }
    }
    return corners;
}

glm::mat4 CascadedShadowMap::buildCascadeMatrix(const glm::vec3& lightDir,
                                                  const std::vector<glm::vec4>& corners) const {
    glm::vec3 center(0.0f);
    for (const auto& c : corners) {
        center += glm::vec3(c);
    }
    center /= static_cast<float>(corners.size());

    const glm::vec3 up = std::abs(lightDir.y) > 0.97f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(center - lightDir, center, up);

    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;
    for (const auto& c : corners) {
        const glm::vec4 lc = lightView * c;
        minX = std::min(minX, lc.x); maxX = std::max(maxX, lc.x);
        minY = std::min(minY, lc.y); maxY = std::max(maxY, lc.y);
        minZ = std::min(minZ, lc.z); maxZ = std::max(maxZ, lc.z);
    }

    const float extentX = maxX - minX;
    const float extentY = maxY - minY;
    const int resolution = std::max(resolution_, 1);
    const float texelSizeX = extentX / static_cast<float>(resolution);
    const float texelSizeY = extentY / static_cast<float>(resolution);
    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    if (texelSizeX > 0.0f) {
        centerX = std::round(centerX / texelSizeX) * texelSizeX;
    }
    if (texelSizeY > 0.0f) {
        centerY = std::round(centerY / texelSizeY) * texelSizeY;
    }
    minX = centerX - extentX * 0.5f;
    maxX = centerX + extentX * 0.5f;
    minY = centerY - extentY * 0.5f;
    maxY = centerY + extentY * 0.5f;

    // Pull near plane back to capture shadow casters outside the frustum
    constexpr float zMult = 10.0f;
    if (minZ < 0) { minZ *= zMult; } else { minZ /= zMult; }
    if (maxZ < 0) { maxZ /= zMult; } else { maxZ *= zMult; }

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}
