#include "engine/rendering/lighting/ShadowMap.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

ShadowMap::~ShadowMap() {
    destroy();
}

void ShadowMap::create(int resolution) {
    resolution_ = resolution;

    glGenTextures(1, &depthTex_);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 resolution,
                 resolution,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("Shadow framebuffer is not complete: {:#x}", static_cast<unsigned>(status));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Shadow framebuffer creation failed");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::resize(int resolution) {
    if (resolution == resolution_) {
        return;
    }
    destroy();
    create(resolution);
}

void ShadowMap::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void ShadowMap::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::destroy() {
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (depthTex_ != 0) {
        glDeleteTextures(1, &depthTex_);
        depthTex_ = 0;
    }
    resolution_ = 0;
}
