#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

class DitherPass {
public:
    DitherPass();
    ~DitherPass();

    // Non-copyable
    DitherPass(const DitherPass&) = delete;
    DitherPass& operator=(const DitherPass&) = delete;

    void apply(GLuint sceneColorTex, const glm::mat4& inverseView,
               float thresholdBias, int displayW, int displayH);

private:
    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
};
