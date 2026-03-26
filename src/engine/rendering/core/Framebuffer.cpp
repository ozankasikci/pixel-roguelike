#include "engine/rendering/core/Framebuffer.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

Framebuffer::~Framebuffer() {
    destroy();
}

void Framebuffer::create(int width, int height) {
    width_ = width;
    height_ = height;

    // Color attachment 0: GL_RGB16F (linear luminance)
    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Color attachment 1: GL_RGBA16F (world-space normals + material marker for post-process palette decisions)
    glGenTextures(1, &normalTex_);
    glBindTexture(GL_TEXTURE_2D, normalTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Depth attachment: texture (so dither pass can sample it for edge detection + fog)
    glGenTextures(1, &depthTex_);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Assemble FBO with MRT
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("Framebuffer is not complete: {:#x}", static_cast<unsigned>(status));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Framebuffer creation failed");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    spdlog::info("Framebuffer created: {}x{} GL_RGB16F + RGBA normals/material + depth", width, height);
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::resize(int w, int h) {
    destroy();
    create(w, h);
}

void Framebuffer::destroy() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (colorTex_) {
        glDeleteTextures(1, &colorTex_);
        colorTex_ = 0;
    }
    if (normalTex_) {
        glDeleteTextures(1, &normalTex_);
        normalTex_ = 0;
    }
    if (depthTex_) {
        glDeleteTextures(1, &depthTex_);
        depthTex_ = 0;
    }
}
