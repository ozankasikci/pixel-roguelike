#include "engine/rendering/post/StylizePass.h"
#include "engine/rendering/core/Shader.h"

#include <spdlog/spdlog.h>

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

} // namespace

StylizePass::StylizePass() {
    shader_ = std::make_unique<Shader>(
        "assets/shaders/engine/stylize.vert",
        "assets/shaders/engine/stylize.frag"
    );
    createFullscreenQuad(quadVAO_, quadVBO_);
    spdlog::info("StylizePass initialized");
}

StylizePass::~StylizePass() {
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
}

void StylizePass::apply(GLuint compositeColorTex,
                        GLuint sceneColorTex,
                        GLuint sceneDepthTex,
                        GLuint sceneNormalTex,
                        const PostProcessParams& params,
                        int displayW,
                        int displayH,
                        GLuint targetFbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, displayW, displayH);
    glDisable(GL_DEPTH_TEST);

    shader_->use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, compositeColorTex);
    shader_->setInt("compositeColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    shader_->setInt("sceneColor", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    shader_->setInt("sceneDepth", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    shader_->setInt("sceneNormal", 3);

    shader_->setInt("uEnableDither", params.enableDither ? 1 : 0);
    shader_->setInt("uEnableEdges", params.enableEdges ? 1 : 0);
    shader_->setInt("uDebugViewMode", params.debugViewMode);
    shader_->setInt("uPaletteVariant", params.paletteVariant);
    shader_->setFloat("uThresholdBias", params.thresholdBias);
    shader_->setFloat("uPatternScale", params.patternScale);
    shader_->setFloat("uEdgeThreshold", params.edgeThreshold);
    shader_->setFloat("uDepthViewScale", params.depthViewScale);
    shader_->setFloat("uFogStart", params.fogStart);
    shader_->setFloat("uNearPlane", params.nearPlane);
    shader_->setFloat("uFarPlane", params.farPlane);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
