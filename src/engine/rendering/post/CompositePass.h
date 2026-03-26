#pragma once

#include "engine/rendering/post/PostProcessParams.h"

#include <glad/gl.h>
#include <memory>

class Shader;

class CompositePass {
public:
    CompositePass();
    ~CompositePass();

    CompositePass(const CompositePass&) = delete;
    CompositePass& operator=(const CompositePass&) = delete;

    void apply(GLuint sceneColorTex,
               GLuint sceneDepthTex,
               GLuint sceneNormalTex,
               GLuint targetFbo,
               const PostProcessParams& params,
               int targetW,
               int targetH);

private:
    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
};
