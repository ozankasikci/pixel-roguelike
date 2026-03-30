#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

class Shader;

class CascadedShadowMap {
public:
    static constexpr int kCascadeCount = 3;
    static constexpr int kDefaultResolution = 1024;

    CascadedShadowMap() = default;
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    void create(int resolution = kDefaultResolution);
    void destroy();
    void bind() const;
    void unbind() const;

    // Compute cascade matrices using PSSM split blending.
    // lambda=0 produces uniform linear splits; lambda=1 produces fully logarithmic splits.
    void computeCascades(const glm::mat4& viewMatrix,
                         const glm::mat4& projectionMatrix,
                         const glm::vec3& lightDirection,
                         float nearPlane, float farPlane,
                         float lambda = 0.5f);

    GLuint depthArrayTexture() const { return depthArray_; }
    GLuint framebuffer() const { return fbo_; }
    int resolution() const { return resolution_; }
    const std::array<glm::mat4, kCascadeCount>& lightSpaceMatrices() const { return lightSpaceMatrices_; }
    const std::array<float, kCascadeCount>& splitDistances() const { return splitDistances_; }

private:
    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj,
                                                        const glm::mat4& view) const;
    glm::mat4 buildCascadeMatrix(const glm::vec3& lightDir,
                                  const std::vector<glm::vec4>& corners) const;

    GLuint fbo_ = 0;
    GLuint depthArray_ = 0;     // GL_TEXTURE_2D_ARRAY
    int resolution_ = 0;
    std::array<glm::mat4, kCascadeCount> lightSpaceMatrices_{};
    std::array<float, kCascadeCount> splitDistances_{};
};
