#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

struct DitherParams {
    float thresholdBias = 0.028f;
    float patternScale  = 246.519f;
    float edgeThreshold = 0.17f;
    float fogDensity    = 0.145f;
    float fogStart      = 8.5f;
    float nearPlane     = 0.1f;
    float farPlane      = 100.0f;
};

class DitherPass {
public:
    DitherPass();
    ~DitherPass();

    // Non-copyable
    DitherPass(const DitherPass&) = delete;
    DitherPass& operator=(const DitherPass&) = delete;

    void apply(GLuint sceneColorTex, GLuint sceneDepthTex, GLuint sceneNormalTex,
               const DitherParams& params,
               int displayW, int displayH);

private:
    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
};
