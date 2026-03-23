#pragma once

#include <glad/gl.h>

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    // Non-copyable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void create(int width, int height);
    void bind();
    void unbind();
    void resize(int w, int h);

    GLuint colorTexture() const { return colorTex_; }
    GLuint depthTexture() const { return depthTex_; }
    GLuint normalTexture() const { return normalTex_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    void destroy();

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthTex_ = 0;
    GLuint normalTex_ = 0;
    int width_ = 0;
    int height_ = 0;
};
