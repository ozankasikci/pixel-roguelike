#include "engine/rendering/SceneRenderPipeline.h"

#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/lighting/CascadedShadowMap.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kShadowResolutions[] = {512, 1024, 2048};
constexpr int kCsmTextureUnit = 16;
constexpr int kLtcMatUnit = 10;
constexpr int kLtcAmpUnit = 11;

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

} // namespace

void SceneRenderPipeline::init() {
    sceneShader_ = std::make_unique<Shader>("assets/shaders/game/scene.vert",
                                            "assets/shaders/game/scene.frag");
    shadowShader_ = std::make_unique<Shader>("assets/shaders/engine/shadow_depth.vert",
                                              "assets/shaders/engine/shadow_depth.frag");
    csmShader_ = std::make_unique<Shader>("assets/shaders/engine/csm_depth.vert",
                                           "assets/shaders/engine/csm_depth.geom",
                                           "assets/shaders/engine/csm_depth.frag");
    csmShadowMap_.create(CascadedShadowMap::kDefaultResolution);
    renderer_ = std::make_unique<Renderer>(sceneShader_.get());
    bloomPass_.init();
    ssaoPass_.init();
    ltcData_.init();
    ensureFramebuffers(1280, 720);
}

void SceneRenderPipeline::shutdown() {
    renderer_.reset();
    shadowShader_.reset();
    sceneShader_.reset();
}

void SceneRenderPipeline::render(const SceneRenderInput& input,
                                  int internalWidth,
                                  int internalHeight,
                                  int outputWidth,
                                  int outputHeight,
                                  GLuint targetFramebuffer) {
    const double t0 = glfwGetTime();
    ensureFramebuffers(internalWidth, internalHeight);

    // Make mutable copy of lights for shadow slot assignment
    std::vector<RenderLight> lights = *input.lights;
    assignShadowSlots(lights, input.shadowsEnabled);

    ShadowRenderData shadowData;
    const double tShadowStart = glfwGetTime();
    renderShadowPass(*input.objects, lights, input, shadowData);
    const double tShadowEnd = glfwGetTime();

    const double tSceneStart = glfwGetTime();
    renderScenePass(input, lights, shadowData, internalWidth, internalHeight);
    const double tSceneEnd = glfwGetTime();

    renderPostProcess(input, outputWidth, outputHeight, targetFramebuffer);

    const double tEnd = glfwGetTime();
    lastStats_.totalRenderMs = (tEnd - t0) * 1000.0;
    lastStats_.shadowPassMs = (tShadowEnd - tShadowStart) * 1000.0;
    lastStats_.scenePassMs = (tSceneEnd - tSceneStart) * 1000.0;
    lastStats_.objectCount = static_cast<int>(input.objects->size());
    lastStats_.lightCount = static_cast<int>(lights.size());
    lastStats_.drawCalls = lastStats_.objectCount;
}

void SceneRenderPipeline::assignShadowSlots(std::vector<RenderLight>& lights, bool enabled) {
    for (auto& light : lights) {
        light.shadowIndex = -1;
    }
    if (!enabled) {
        return;
    }

    int nextShadowIndex = 0;
    for (auto& light : lights) {
        if (nextShadowIndex >= kMaxShadowedSpotLights) {
            break;
        }
        if (light.type != LightType::Spot || !light.castsShadows) {
            continue;
        }
        light.shadowIndex = nextShadowIndex++;
    }
}

glm::mat4 SceneRenderPipeline::buildShadowMatrix(const RenderLight& light) const {
    const glm::vec3 direction = safeNormalize(light.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::vec3 up = std::abs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.97f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(light.position, light.position + direction, up);
    const float coneDegrees = std::max(light.outerConeDegrees, light.innerConeDegrees + 1.0f) * 2.0f + 4.0f;
    const float farPlane = std::max(light.radius, 0.5f);
    const glm::mat4 lightProjection = glm::perspective(glm::radians(coneDegrees), 1.0f, 0.05f, farPlane);
    return lightProjection * lightView;
}

void SceneRenderPipeline::renderShadowPass(const std::vector<RenderObject>& objects,
                                            std::vector<RenderLight>& lights,
                                            const SceneRenderInput& input,
                                            ShadowRenderData& shadowData) {
    shadowData.shadowCount = 0;
    shadowData.matrices.fill(glm::mat4(1.0f));
    for (std::size_t i = 0; i < shadowMaps_.size(); ++i) {
        shadowData.textures[i] = shadowMaps_[i].texture();
    }

    if (!input.shadowsEnabled || shadowShader_ == nullptr) {
        return;
    }

    const int desiredResolution = kShadowResolutions[std::clamp(input.shadowResolutionIndex, 0, 2)];
    for (auto& shadowMap : shadowMaps_) {
        if (shadowMap.texture() == 0) {
            shadowMap.create(desiredResolution);
        } else {
            shadowMap.resize(desiredResolution);
        }
    }

    glEnable(GL_DEPTH_TEST);
    shadowShader_->use();

    for (const auto& light : lights) {
        if (light.shadowIndex < 0 || light.shadowIndex >= kMaxShadowedSpotLights) {
            continue;
        }

        ShadowMap& shadowMap = shadowMaps_[static_cast<std::size_t>(light.shadowIndex)];
        const glm::mat4 lightMatrix = buildShadowMatrix(light);
        shadowData.matrices[static_cast<std::size_t>(light.shadowIndex)] = lightMatrix;
        shadowData.textures[static_cast<std::size_t>(light.shadowIndex)] = shadowMap.texture();
        shadowData.shadowCount = std::max(shadowData.shadowCount, light.shadowIndex + 1);

        shadowMap.bind();
        glViewport(0, 0, desiredResolution, desiredResolution);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader_->setMat4("uLightViewProjection", lightMatrix);

        for (const auto& object : objects) {
            shadowShader_->setMat4("uModel", object.modelMatrix);
            object.mesh->draw();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Render CSM for directional sun
    const LightingEnvironment& lighting = input.lightingEnvironment;
    if (lighting.sun.enabled && lighting.enableShadows && csmShader_ != nullptr) {
        csmShadowMap_.computeCascades(input.viewMatrix,
                                       input.projectionMatrix,
                                       lighting.sun.direction,
                                       input.nearPlane,
                                       input.farPlane,
                                       input.postParams ? input.postParams->csmLambda : 0.5f);

        csmShadowMap_.bind();
        csmShader_->use();

        const auto& csmMatrices = csmShadowMap_.lightSpaceMatrices();
        for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
            csmShader_->setMat4("uLightSpaceMatrices[" + std::to_string(i) + "]", csmMatrices[i]);
        }

        for (const auto& object : objects) {
            csmShader_->setMat4("uModel", object.modelMatrix);
            object.mesh->draw();
        }

        csmShadowMap_.unbind();
    }
}

void SceneRenderPipeline::renderScenePass(const SceneRenderInput& input,
                                           const std::vector<RenderLight>& lights,
                                           const ShadowRenderData& shadowData,
                                           int internalWidth,
                                           int internalHeight) {
    ensureFramebuffers(internalWidth, internalHeight);

    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    const LightingEnvironment& lighting = input.lightingEnvironment;
    const float timeSeconds = static_cast<float>(glfwGetTime());

    // Bind CSM depth array texture and set scene shader uniforms for CSM
    const bool csmEnabled = lighting.sun.enabled && lighting.enableShadows;
    sceneShader_->use();
    sceneShader_->setFloat("uTimeSeconds", timeSeconds);
    sceneShader_->setMat4("uViewMatrix", input.viewMatrix);
    sceneShader_->setInt("uCsmShadowMap", kCsmTextureUnit);
    sceneShader_->setInt("uCsmEnabled", csmEnabled ? 1 : 0);
    sceneShader_->setInt("uCsmCascadeCount", CascadedShadowMap::kCascadeCount);
    if (csmEnabled) {
        const auto& csmMatrices = csmShadowMap_.lightSpaceMatrices();
        const auto& csmSplits = csmShadowMap_.splitDistances();
        for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
            sceneShader_->setMat4("uCsmMatrices[" + std::to_string(i) + "]", csmMatrices[i]);
            sceneShader_->setFloat("uCsmSplitDistances[" + std::to_string(i) + "]", csmSplits[i]);
        }
    }
    glActiveTexture(GL_TEXTURE0 + kCsmTextureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, csmEnabled ? csmShadowMap_.depthArrayTexture() : 0);

    // Bind LTC lookup textures to units 10 and 11 (always real textures -- per Pitfall 1)
    glActiveTexture(GL_TEXTURE0 + kLtcMatUnit);
    glBindTexture(GL_TEXTURE_2D, ltcData_.ltcMatTexture());
    sceneShader_->setInt("uLtcMat", kLtcMatUnit);
    glActiveTexture(GL_TEXTURE0 + kLtcAmpUnit);
    glBindTexture(GL_TEXTURE_2D, ltcData_.ltcAmpTexture());
    sceneShader_->setInt("uLtcAmp", kLtcAmpUnit);
    glActiveTexture(GL_TEXTURE0);

    renderer_->drawScene(*input.objects,
                          lights,
                          lighting,
                          shadowData,
                          input.viewMatrix,
                          input.projectionMatrix,
                          input.cameraPosition);

    // Render viewmodel objects with depth-range trick to prevent z-fighting with world geometry
    if (input.viewmodelObjects != nullptr && !input.viewmodelObjects->empty()) {
        glDepthRange(0.0, 0.01);
        sceneShader_->use();
        sceneShader_->setFloat("uTimeSeconds", timeSeconds);
        // CSM uniforms carry over since same shader is active; viewmodel objects don't receive CSM shadows
        renderer_->drawScene(*input.viewmodelObjects,
                              lights,
                              lighting,
                              shadowData,
                              input.viewMatrix,
                              input.projectionMatrix,
                              input.cameraPosition);
        glDepthRange(0.0, 1.0);
    }

    sceneFBO_.unbind();
    glDisable(GL_DEPTH_TEST);
}

void SceneRenderPipeline::renderPostProcess(const SceneRenderInput& input,
                                             int outputWidth,
                                             int outputHeight,
                                             GLuint targetFramebuffer) {
    // Build a mutable local copy of PostProcessParams so we can set per-frame fields
    PostProcessParams post = *input.postParams;
    post.nearPlane = input.nearPlane;
    post.farPlane = input.farPlane;
    post.timeSeconds = static_cast<float>(glfwGetTime());
    post.inverseViewProjection = glm::inverse(input.projectionMatrix * input.viewMatrix);

    // Sync sky sun direction from the lighting environment (equivalent to syncSkySunFromDirectional)
    const glm::vec3& sunDir = input.lightingEnvironment.sun.direction;
    if (glm::dot(sunDir, sunDir) > 0.0001f) {
        post.sky.sunDirection = glm::normalize(sunDir);
    }
    post.sky.sunColor = glm::max(input.lightingEnvironment.sun.color, glm::vec3(0.0f));

    const double tBloomStart = glfwGetTime();
    bloomPass_.render(sceneFBO_.colorTexture(), post.bloomRadius * 0.003f, post.bloomThreshold);
    lastStats_.bloomMs = (glfwGetTime() - tBloomStart) * 1000.0;

    const double tSsaoStart = glfwGetTime();
    if (post.enableSsao) {
        // Use geomNormalTexture() for SSAO (geometric normals, per Pitfall 5)
        ssaoPass_.render(sceneFBO_.depthTexture(),
                          sceneFBO_.geomNormalTexture(),
                          input.projectionMatrix,
                          input.viewMatrix,
                          post.ssaoRadius,
                          post.ssaoBias,
                          post.ssaoStrength);
    }
    lastStats_.ssaoMs = (glfwGetTime() - tSsaoStart) * 1000.0;

    const double tCompositeStart = glfwGetTime();
    compositePass_.apply(sceneFBO_.colorTexture(),
                          sceneFBO_.depthTexture(),
                          sceneFBO_.normalTexture(),
                          bloomPass_.bloomTexture(),
                          post.enableSsao ? ssaoPass_.aoTexture() : 0,
                          compositeFBO_.framebuffer(),
                          post,
                          compositeFBO_.width(),
                          compositeFBO_.height());

    stylizePass_.apply(compositeFBO_.colorTexture(),
                        sceneFBO_.colorTexture(),
                        sceneFBO_.depthTexture(),
                        sceneFBO_.normalTexture(),
                        post,
                        outputWidth,
                        outputHeight,
                        targetFramebuffer);
    lastStats_.compositeMs = (glfwGetTime() - tCompositeStart) * 1000.0;
}

void SceneRenderPipeline::ensureFramebuffers(int internalWidth, int internalHeight) {
    const int safeW = std::max(internalWidth, 1);
    const int safeH = std::max(internalHeight, 1);
    if (sceneFBO_.framebuffer() == 0) {
        sceneFBO_.create(safeW, safeH);
    } else if (sceneFBO_.width() != safeW || sceneFBO_.height() != safeH) {
        sceneFBO_.resize(safeW, safeH);
    }

    if (compositeFBO_.framebuffer() == 0) {
        compositeFBO_.create(safeW, safeH);
    } else if (compositeFBO_.width() != safeW || compositeFBO_.height() != safeH) {
        compositeFBO_.resize(safeW, safeH);
    }

    bloomPass_.resize(safeW, safeH);
    ssaoPass_.resize(safeW, safeH);
}
