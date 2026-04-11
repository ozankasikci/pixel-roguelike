#pragma once

#include "engine/particles/ParticleTypes.h"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <memory>
#include <vector>

class Shader;

namespace particles {

class ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    void init();
    void shutdown();

    void render(const std::vector<ParticleRenderBatch>& batches,
                GLuint sceneDepthTexture,
                const glm::mat4& view,
                const glm::mat4& projection,
                float nearPlane, float farPlane,
                int viewportWidth, int viewportHeight);

private:
    void setupQuadVAO();
    void drawBatch(const ParticleRenderBatch& batch);

    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    GLuint instanceVBO_ = 0;
};

} // namespace particles
