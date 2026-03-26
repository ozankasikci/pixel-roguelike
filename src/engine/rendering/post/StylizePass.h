#pragma once

#include "engine/rendering/post/PostProcessParams.h"

#include <glad/gl.h>
#include <memory>

class Shader;

class StylizePass {
public:
    StylizePass();
    ~StylizePass();

    StylizePass(const StylizePass&) = delete;
    StylizePass& operator=(const StylizePass&) = delete;

    void apply(GLuint compositeColorTex,
               GLuint sceneColorTex,
               GLuint sceneDepthTex,
               GLuint sceneNormalTex,
               const PostProcessParams& params,
               int displayW,
               int displayH);

private:
    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
};
