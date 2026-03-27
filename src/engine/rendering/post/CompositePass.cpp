#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/core/Shader.h"

#include <spdlog/spdlog.h>

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

CompositePass::CompositePass() {
    shader_ = std::make_unique<Shader>(
        "assets/shaders/engine/composite.vert",
        "assets/shaders/engine/composite.frag"
    );
    createFullscreenQuad(quadVAO_, quadVBO_);
    spdlog::info("CompositePass initialized");
}

CompositePass::~CompositePass() {
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
}

void CompositePass::apply(GLuint sceneColorTex,
                          GLuint sceneDepthTex,
                          GLuint sceneNormalTex,
                          GLuint targetFbo,
                          const PostProcessParams& params,
                          int targetW,
                          int targetH) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, targetW, targetH);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    shader_->use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    shader_->setInt("sceneColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    shader_->setInt("sceneDepth", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    shader_->setInt("sceneNormal", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, skyTextures_.resolve(params.sky.cloudLayerAPath));
    shader_->setInt("uCloudLayerA", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, skyTextures_.resolve(params.sky.cloudLayerBPath));
    shader_->setInt("uCloudLayerB", 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, skyTextures_.resolve(params.sky.horizonLayerPath));
    shader_->setInt("uHorizonLayer", 5);

    shader_->setInt("uEnableSky", (params.enableSky && params.sky.enabled) ? 1 : 0);
    shader_->setInt("uEnableFog", params.enableFog ? 1 : 0);
    shader_->setInt("uEnableToneMap", params.enableToneMap ? 1 : 0);
    shader_->setInt("uEnableBloom", params.enableBloom ? 1 : 0);
    shader_->setInt("uEnableVignette", params.enableVignette ? 1 : 0);
    shader_->setInt("uEnableGrain", params.enableGrain ? 1 : 0);
    shader_->setInt("uEnableScanlines", params.enableScanlines ? 1 : 0);
    shader_->setInt("uEnableSharpen", params.enableSharpen ? 1 : 0);
    shader_->setInt("uToneMapMode", params.toneMapMode);
    shader_->setInt("uDebugViewMode", params.debugViewMode);
    shader_->setFloat("uFogDensity", params.fogDensity);
    shader_->setFloat("uFogStart", params.fogStart);
    shader_->setFloat("uExposure", params.exposure);
    shader_->setFloat("uGamma", params.gamma);
    shader_->setFloat("uContrast", params.contrast);
    shader_->setFloat("uSaturation", params.saturation);
    shader_->setFloat("uBloomThreshold", params.bloomThreshold);
    shader_->setFloat("uBloomIntensity", params.bloomIntensity);
    shader_->setFloat("uBloomRadius", params.bloomRadius);
    shader_->setFloat("uVignetteStrength", params.vignetteStrength);
    shader_->setFloat("uVignetteSoftness", params.vignetteSoftness);
    shader_->setFloat("uGrainAmount", params.grainAmount);
    shader_->setFloat("uScanlineAmount", params.scanlineAmount);
    shader_->setFloat("uScanlineDensity", params.scanlineDensity);
    shader_->setFloat("uSharpenAmount", params.sharpenAmount);
    shader_->setFloat("uSplitToneStrength", params.splitToneStrength);
    shader_->setFloat("uSplitToneBalance", params.splitToneBalance);
    shader_->setFloat("uDepthViewScale", params.depthViewScale);
    shader_->setFloat("uTimeSeconds", params.timeSeconds);
    shader_->setVec3("uFogNearColor", params.fogNearColor);
    shader_->setVec3("uFogFarColor", params.fogFarColor);
    shader_->setVec3("uShadowTint", params.shadowTint);
    shader_->setVec3("uHighlightTint", params.highlightTint);
    shader_->setFloat("uNearPlane", params.nearPlane);
    shader_->setFloat("uFarPlane", params.farPlane);
    shader_->setMat4("uInverseViewProjection", params.inverseViewProjection);
    shader_->setVec3("uSkyZenithColor", params.sky.zenithColor);
    shader_->setVec3("uSkyHorizonColor", params.sky.horizonColor);
    shader_->setVec3("uSkyGroundHazeColor", params.sky.groundHazeColor);
    shader_->setVec3("uSunDirection", params.sky.sunDirection);
    shader_->setVec3("uSunColor", params.sky.sunColor);
    shader_->setFloat("uSunSize", params.sky.sunSize);
    shader_->setFloat("uSunGlow", params.sky.sunGlow);
    shader_->setInt("uMoonEnabled", params.sky.moonEnabled ? 1 : 0);
    shader_->setVec3("uMoonDirection", params.sky.moonDirection);
    shader_->setVec3("uMoonColor", params.sky.moonColor);
    shader_->setFloat("uMoonSize", params.sky.moonSize);
    shader_->setFloat("uMoonGlow", params.sky.moonGlow);
    shader_->setVec3("uCloudTint", params.sky.cloudTint);
    shader_->setFloat("uCloudScale", params.sky.cloudScale);
    shader_->setFloat("uCloudSpeed", params.sky.cloudSpeed);
    shader_->setFloat("uCloudCoverage", params.sky.cloudCoverage);
    shader_->setFloat("uCloudParallax", params.sky.cloudParallax);
    shader_->setVec3("uHorizonTint", params.sky.horizonTint);
    shader_->setFloat("uHorizonHeight", params.sky.horizonHeight);
    shader_->setFloat("uHorizonFade", params.sky.horizonFade);
    shader_->setInt("uHasCloudLayerA", params.sky.cloudLayerAPath.empty() ? 0 : 1);
    shader_->setInt("uHasCloudLayerB", params.sky.cloudLayerBPath.empty() ? 0 : 1);
    shader_->setInt("uHasHorizonLayer", params.sky.horizonLayerPath.empty() ? 0 : 1);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
}
