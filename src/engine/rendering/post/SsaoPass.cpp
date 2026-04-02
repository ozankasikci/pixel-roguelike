#include "engine/rendering/post/SsaoPass.h"
#include "engine/rendering/core/Shader.h"

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <random>

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

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

} // namespace

SsaoPass::SsaoPass() = default;

SsaoPass::~SsaoPass() {
    destroy();
}

void SsaoPass::init() {
    ssaoShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/ssao.vert",
        "assets/shaders/engine/ssao.frag"
    );
    blurShader_ = std::make_unique<Shader>(
        "assets/shaders/engine/ssao_blur.vert",
        "assets/shaders/engine/ssao_blur.frag"
    );
    createFullscreenQuad(quadVAO_, quadVBO_);
    generateKernel();
    generateNoiseTexture();
    spdlog::info("SsaoPass initialized with {} kernel samples", kernel_.size());
}

void SsaoPass::generateKernel() {
    // Deterministic seed for stable results (Pitfall 5 from RESEARCH.md)
    std::default_random_engine rng(42);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

    kernel_.clear();
    kernel_.reserve(32);
    for (int i = 0; i < 32; ++i) {
        glm::vec3 sample(
            uniform(rng) * 2.0f - 1.0f,
            uniform(rng) * 2.0f - 1.0f,
            uniform(rng)
        );
        sample = glm::normalize(sample);
        sample *= uniform(rng);
        // Accelerating distribution: more samples near origin
        float scale = static_cast<float>(i) / 32.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        kernel_.push_back(sample);
    }
}

void SsaoPass::generateNoiseTexture() {
    // 4x4 random rotation vectors (z=0) for tile-based TBN rotation
    // Deterministic seed for stable AO (Pitfall 5 from RESEARCH.md)
    std::default_random_engine rng(123);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

    float noiseData[16 * 4]; // 16 RGBA16F texels
    for (int i = 0; i < 16; ++i) {
        noiseData[i * 4 + 0] = uniform(rng) * 2.0f - 1.0f;
        noiseData[i * 4 + 1] = uniform(rng) * 2.0f - 1.0f;
        noiseData[i * 4 + 2] = 0.0f;
        noiseData[i * 4 + 3] = 0.0f;
    }

    glGenTextures(1, &noiseTex_);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, noiseData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SsaoPass::createFbos(int width, int height) {
    width_ = width;
    height_ = height;

    // SSAO FBO — single-channel R8 (AO factor 0..1)
    glGenTextures(1, &ssaoColorTex_);
    glBindTexture(GL_TEXTURE_2D, ssaoColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &ssaoFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorTex_, 0);

    // Blur FBO — same format, LINEAR filter for smooth upsampling when SSAO
    // runs at reduced resolution
    glGenTextures(1, &blurColorTex_);
    glBindTexture(GL_TEXTURE_2D, blurColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &blurFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurColorTex_, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SsaoPass::destroyFbos() {
    if (ssaoFbo_) {
        glDeleteFramebuffers(1, &ssaoFbo_);
        ssaoFbo_ = 0;
    }
    if (ssaoColorTex_) {
        glDeleteTextures(1, &ssaoColorTex_);
        ssaoColorTex_ = 0;
    }
    if (blurFbo_) {
        glDeleteFramebuffers(1, &blurFbo_);
        blurFbo_ = 0;
    }
    if (blurColorTex_) {
        glDeleteTextures(1, &blurColorTex_);
        blurColorTex_ = 0;
    }
}

void SsaoPass::resize(int width, int height) {
    const int safeW = std::max(width, 1);
    const int safeH = std::max(height, 1);
    if (safeW == width_ && safeH == height_) {
        return;
    }
    destroyFbos();
    createFbos(safeW, safeH);
}

void SsaoPass::render(GLuint sceneDepthTex,
                      GLuint sceneGeomNormalTex,
                      const glm::mat4& projection,
                      const glm::mat4& view,
                      float aoRadius,
                      float aoBias,
                      float aoStrength) {
    if (ssaoFbo_ == 0 || ssaoShader_ == nullptr) {
        return;
    }

    const glm::mat4 invProjection = glm::inverse(projection);
    const glm::vec2 noiseScale(
        static_cast<float>(width_) / 4.0f,
        static_cast<float>(height_) / 4.0f
    );

    // Pass 1: SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    ssaoShader_->use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    ssaoShader_->setInt("uDepthTex", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneGeomNormalTex);
    ssaoShader_->setInt("uGeomNormalTex", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    ssaoShader_->setInt("uNoiseTex", 2);

    // Upload kernel samples
    for (int i = 0; i < 32; ++i) {
        const std::string name = "uSamples[" + std::to_string(i) + "]";
        ssaoShader_->setVec3(name, kernel_[static_cast<std::size_t>(i)]);
    }

    ssaoShader_->setMat4("uProjection", projection);
    ssaoShader_->setMat4("uView", view);
    ssaoShader_->setMat4("uInvProjection", invProjection);
    ssaoShader_->setVec2("uNoiseScale", noiseScale);
    ssaoShader_->setFloat("uAoRadius", aoRadius);
    ssaoShader_->setFloat("uAoBias", aoBias);
    ssaoShader_->setFloat("uAoStrength", aoStrength);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Pass 2: Depth-aware bilateral blur
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo_);
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT);

    blurShader_->use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorTex_);
    blurShader_->setInt("uSsaoInput", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    blurShader_->setInt("uDepthTex", 1);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
}

GLuint SsaoPass::aoTexture() const {
    return blurColorTex_;
}

void SsaoPass::destroy() {
    destroyFbos();
    if (noiseTex_) {
        glDeleteTextures(1, &noiseTex_);
        noiseTex_ = 0;
    }
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }
}
