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

    // location 0: position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // location 1: texcoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    spdlog::info("DitherPass initialized");
}

DitherPass::~DitherPass() {
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
    }
}

void DitherPass::apply(GLuint sceneColorTex, const glm::mat4& inverseView,
                       float thresholdBias, int displayW, int displayH,
                       float patternScale) {
    shader_->use();

    // Bind scene color texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    shader_->setInt("sceneColor", 0);

    // Set world-space anchoring uniform
    shader_->setMat4("uInverseView", inverseView);
    shader_->setFloat("uThresholdBias", thresholdBias);
    shader_->setFloat("uPatternScale", patternScale);

    // Render fullscreen quad at display resolution
    glViewport(0, 0, displayW, displayH);
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
