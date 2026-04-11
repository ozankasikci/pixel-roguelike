#include "engine/particles/ParticleRenderer.h"

#include "engine/rendering/core/Shader.h"
#include "engine/rendering/TextureUnits.h"

namespace particles {

ParticleRenderer::~ParticleRenderer() { shutdown(); }

void ParticleRenderer::init() {
    shader_ = std::make_unique<Shader>("assets/shaders/game/particles.vert",
                                        "assets/shaders/game/particles.frag");
    setupQuadVAO();
}

void ParticleRenderer::shutdown() {
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
    if (instanceVBO_) {
        glDeleteBuffers(1, &instanceVBO_);
        instanceVBO_ = 0;
    }
    shader_.reset();
}

void ParticleRenderer::setupQuadVAO() {
    float quadVerts[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glGenBuffers(1, &instanceVBO_);
    glBindVertexArray(quadVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, 2048 * sizeof(ParticleInstance), nullptr, GL_STREAM_DRAW);

    const GLsizei stride = sizeof(ParticleInstance);
    // position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, position)));
    glVertexAttribDivisor(1, 1);
    // color (packed RGBA8)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, colorPacked)));
    glVertexAttribDivisor(2, 1);
    // size
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, size)));
    glVertexAttribDivisor(3, 1);
    // rotation
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, rotation)));
    glVertexAttribDivisor(4, 1);
    // normalizedAge
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, normalizedAge)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleRenderer::render(const std::vector<ParticleRenderBatch>& batches,
                               GLuint sceneDepthTexture,
                               const glm::mat4& view, const glm::mat4& projection,
                               float nearPlane, float farPlane,
                               int viewportWidth, int viewportHeight) {
    if (batches.empty() || !shader_) return;

    shader_->use();
    shader_->setMat4("uView", view);
    shader_->setMat4("uProjection", projection);
    shader_->setFloat("uNearPlane", nearPlane);
    shader_->setFloat("uFarPlane", farPlane);
    shader_->setVec2("uViewportSize", glm::vec2(viewportWidth, viewportHeight));

    glActiveTexture(GL_TEXTURE0 + TextureUnits::kParticleDepth);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    shader_->setInt("uSceneDepth", TextureUnits::kParticleDepth);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBindVertexArray(quadVAO_);

    // Pass 1: Additive
    glBlendFunc(GL_ONE, GL_ONE);
    for (const auto& b : batches) {
        if (b.blendMode == BlendMode::Additive && b.count > 0) drawBatch(b);
    }

    // Pass 2: Alpha blend
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& b : batches) {
        if (b.blendMode == BlendMode::AlphaBlend && b.count > 0) drawBatch(b);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleRenderer::drawBatch(const ParticleRenderBatch& batch) {
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    const auto dataSize = static_cast<GLsizeiptr>(batch.count) *
                          static_cast<GLsizeiptr>(sizeof(ParticleInstance));
    glBufferData(GL_ARRAY_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, batch.instances);

    shader_->setFloat("uEmissiveStrength", batch.emissiveStrength);
    shader_->setFloat("uSoftFadeRange", batch.softParticleFade);

    if (batch.texture != 0) {
        glActiveTexture(GL_TEXTURE0 + TextureUnits::kParticleTexture);
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        shader_->setInt("uParticleTexture", TextureUnits::kParticleTexture);
        shader_->setInt("uHasTexture", 1);
    } else {
        shader_->setInt("uHasTexture", 0);
    }

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, batch.count);
}

} // namespace particles
