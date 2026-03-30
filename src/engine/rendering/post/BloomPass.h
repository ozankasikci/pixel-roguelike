#pragma once

#include <glad/gl.h>
#include <memory>

class Shader;

class BloomPass {
public:
    static constexpr int kMipCount = 5;

    BloomPass();
    ~BloomPass();

    BloomPass(const BloomPass&) = delete;
    BloomPass& operator=(const BloomPass&) = delete;

    void init();
    void resize(int baseWidth, int baseHeight);
    void render(GLuint sceneColorTex, float filterRadius);
    GLuint bloomTexture() const;

private:
    void destroy();

    struct MipLevel {
        GLuint fbo = 0;
        GLuint colorTex = 0;
        int width = 0;
        int height = 0;
    };

    std::unique_ptr<Shader> downsampleShader_;
    std::unique_ptr<Shader> upsampleShader_;
    MipLevel mips_[kMipCount];
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    bool initialized_ = false;
};
