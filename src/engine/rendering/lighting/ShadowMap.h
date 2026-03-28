#pragma once

#include <glad/gl.h>

class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void create(int resolution);
    void resize(int resolution);
    void bind() const;
    void unbind() const;

    GLuint framebuffer() const { return fbo_; }
    GLuint texture() const { return depthTex_; }
    int resolution() const { return resolution_; }

private:
    void destroy();

    GLuint fbo_ = 0;
    GLuint depthTex_ = 0;
    int resolution_ = 0;
};
