#include "engine/rendering/post/BloomPass.h"
#include "engine/rendering/core/Shader.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace {

void createFullscreenQuad(GLuint& vao, GLuint& vbo) {
    float quadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace

BloomPass::BloomPass() = default;

BloomPass::~BloomPass() {
    destroy();
}

void BloomPass::init() {
    downsampleShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/bloom_downsample.vert",
        "assets/shaders/engine/bloom_downsample.frag"
    );
    upsampleShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/bloom_upsample.vert",
        "assets/shaders/engine/bloom_upsample.frag"
    );
    createFullscreenQuad(quadVAO_, quadVBO_);
    initialized_ = true;
    spdlog::info("BloomPass initialized");
}

void BloomPass::resize(int baseWidth, int baseHeight) {
    // Destroy existing mip FBOs if any
    for (int i = 0; i < kMipCount; ++i) {
        if (mips_[i].fbo != 0) {
            glDeleteFramebuffers(1, &mips_[i].fbo);
            mips_[i].fbo = 0;
        }
        if (mips_[i].colorTex != 0) {
            glDeleteTextures(1, &mips_[i].colorTex);
            mips_[i].colorTex = 0;
        }
    }

    // Create kMipCount mip levels; mip 0 is half base resolution
    int w = std::max(baseWidth / 2, 1);
    int h = std::max(baseHeight / 2, 1);

    for (int i = 0; i < kMipCount; ++i) {
        mips_[i].width = w;
        mips_[i].height = h;

        // Create HDR texture
        glGenTextures(1, &mips_[i].colorTex);
        glBindTexture(GL_TEXTURE_2D, mips_[i].colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create FBO
        glGenFramebuffers(1, &mips_[i].fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mips_[i].fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mips_[i].colorTex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Halve for next level
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }
}

void BloomPass::render(GLuint sceneColorTex, float filterRadius, float threshold, float softKnee) {
    if (!initialized_) {
        return;
    }
    if (mips_[0].fbo == 0) {
        return;
    }

    glBindVertexArray(quadVAO_);
    glDisable(GL_DEPTH_TEST);

    // --- Downsample pass ---
    downsampleShader_->use();
    for (int i = 0; i < kMipCount; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, mips_[i].fbo);
        glViewport(0, 0, mips_[i].width, mips_[i].height);

        GLuint srcTex = (i == 0) ? sceneColorTex : mips_[i - 1].colorTex;
        int srcW = (i == 0) ? mips_[0].width * 2 : mips_[i - 1].width;
        int srcH = (i == 0) ? mips_[0].height * 2 : mips_[i - 1].height;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        downsampleShader_->setInt("uSrcTex", 0);
        downsampleShader_->setVec2("uSrcTexelSize",
            glm::vec2(1.0f / static_cast<float>(srcW),
                      1.0f / static_cast<float>(srcH)));

        // Apply threshold prefilter only on first downsample pass
        downsampleShader_->setFloat("uThreshold", (i == 0) ? threshold : 0.0f);
        downsampleShader_->setFloat("uSoftKnee", softKnee);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // --- Upsample pass (additive blend, bottom mip up to mip 0) ---
    upsampleShader_->use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    for (int i = kMipCount - 2; i >= 0; --i) {
        glBindFramebuffer(GL_FRAMEBUFFER, mips_[i].fbo);
        glViewport(0, 0, mips_[i].width, mips_[i].height);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mips_[i + 1].colorTex);
        upsampleShader_->setInt("uSrcTex", 0);
        upsampleShader_->setFloat("uFilterRadius", filterRadius);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BloomPass::bloomTexture() const {
    return mips_[0].colorTex;
}

void BloomPass::destroy() {
    for (int i = 0; i < kMipCount; ++i) {
        if (mips_[i].fbo != 0) {
            glDeleteFramebuffers(1, &mips_[i].fbo);
            mips_[i].fbo = 0;
        }
        if (mips_[i].colorTex != 0) {
            glDeleteTextures(1, &mips_[i].colorTex);
            mips_[i].colorTex = 0;
        }
    }
    if (quadVAO_ != 0) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_ != 0) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
}
