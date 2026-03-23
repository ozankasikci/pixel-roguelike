#include "DitherPass.h"
#include "Shader.h"

#include <spdlog/spdlog.h>

DitherPass::DitherPass() {
    shader_ = std::make_unique<Shader>(
        "assets/shaders/dither.vert",
        "assets/shaders/dither.frag"
    );

    // Fullscreen quad: position (vec2) + texcoord (vec2)
    float quadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);

    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    spdlog::info("DitherPass initialized (edge detection + fog)");
}

DitherPass::~DitherPass() {
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
}

void DitherPass::apply(GLuint sceneColorTex, GLuint sceneDepthTex, GLuint sceneNormalTex,
                       const DitherParams& params,
                       int displayW, int displayH) {
    shader_->use();

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    shader_->setInt("sceneColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    shader_->setInt("sceneDepth", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    shader_->setInt("sceneNormal", 2);

    // Set uniforms
    shader_->setFloat("uThresholdBias", params.thresholdBias);
    shader_->setFloat("uEdgeThreshold", params.edgeThreshold);
    shader_->setFloat("uFogDensity", params.fogDensity);
    shader_->setFloat("uFogStart", params.fogStart);
    shader_->setFloat("uNearPlane", params.nearPlane);
    shader_->setFloat("uFarPlane", params.farPlane);

    glViewport(0, 0, displayW, displayH);
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
