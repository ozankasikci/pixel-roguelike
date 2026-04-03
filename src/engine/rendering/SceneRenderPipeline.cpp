#include "engine/rendering/SceneRenderPipeline.h"

#include "engine/core/MathUtils.h"
#include "engine/rendering/FrustumCulling.h"
#include "engine/rendering/TextureUnits.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/lighting/CascadedShadowMap.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kShadowResolutions[] = {512, 1024, 2048};

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
    skyTextures_.initReflectionResources();
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
    ensureFramebuffers(internalWidth, internalHeight, input.postParams);

    // Frustum culling (D-01: AABB only, D-02: at pipeline level)
    const glm::mat4 vp = input.projectionMatrix * input.viewMatrix;
    const auto frustumPlanes = extractFrustumPlanes(vp);
    std::vector<RenderObject> culledObjects;
    culledObjects.reserve(input.objects->size());
    for (const auto& obj : *input.objects) {
        if (obj.mesh && isAabbInsideFrustum(obj.mesh->aabbMin(), obj.mesh->aabbMax(),
                                             obj.modelMatrix, frustumPlanes)) {
            culledObjects.push_back(obj);
        } else if (!obj.mesh) {
            culledObjects.push_back(obj);  // Keep objects without mesh (safety)
        }
    }

    // Sort: group by material, then front-to-back within each group
    const glm::vec3 camPos = input.cameraPosition;
    const glm::vec3 forward = glm::normalize(glm::vec3(input.viewMatrix[0][2],
                                                         input.viewMatrix[1][2],
                                                         input.viewMatrix[2][2]));
    std::sort(culledObjects.begin(), culledObjects.end(),
        [&](const RenderObject& a, const RenderObject& b) {
            if (a.material.id != b.material.id) return a.material.id < b.material.id;
            float distA = glm::dot(camPos - glm::vec3(a.modelMatrix[3]), forward);
            float distB = glm::dot(camPos - glm::vec3(b.modelMatrix[3]), forward);
            return distA < distB;
        });

    // Create a view of the input with culled objects
    SceneRenderInput culledInput = input;
    culledInput.objects = &culledObjects;

    // Make mutable copy of lights for shadow slot assignment
    std::vector<RenderLight> lights = *input.lights;
    assignShadowSlots(lights, input.shadowsEnabled);

    ShadowRenderData shadowData;
    const double tShadowStart = glfwGetTime();
    // Shadow pass uses UNCULLED objects — shadow casters behind the camera
    // may still cast shadows into the visible scene.
    renderShadowPass(*input.objects, lights, input, shadowData);
    const double tShadowEnd = glfwGetTime();

    const double tSceneStart = glfwGetTime();
    renderScenePass(culledInput, lights, shadowData, internalWidth, internalHeight);
    const double tSceneEnd = glfwGetTime();

    renderPostProcess(culledInput, outputWidth, outputHeight, targetFramebuffer);

    const double tEnd = glfwGetTime();
    lastStats_.totalRenderMs = (tEnd - t0) * 1000.0;
    lastStats_.shadowPassMs = (tShadowEnd - tShadowStart) * 1000.0;
    lastStats_.scenePassMs = (tSceneEnd - tSceneStart) * 1000.0;
    lastStats_.objectCount = static_cast<int>(input.objects->size());
    lastStats_.lightCount = static_cast<int>(lights.size());
    lastStats_.drawCalls = static_cast<int>(culledObjects.size());
    lastStats_.culledCount = static_cast<int>(input.objects->size()) - static_cast<int>(culledObjects.size());
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
    int shadowCulled = 0;

    shadowData.shadowCount = 0;
    shadowData.matrices.fill(glm::mat4(1.0f));
    for (std::size_t i = 0; i < shadowMaps_.size(); ++i) {
        shadowData.textures[i] = shadowMaps_[i].texture();
    }

    if (!input.shadowsEnabled || shadowShader_ == nullptr) {
        lastStats_.shadowCulledCount = 0;
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

        // Frustum cull objects against this spot light's projection
        const auto lightFrustum = extractFrustumPlanes(lightMatrix);
        for (const auto& object : objects) {
            if (object.mesh && !isAabbInsideFrustum(object.mesh->aabbMin(), object.mesh->aabbMax(),
                                                     object.modelMatrix, lightFrustum)) {
                ++shadowCulled;
                continue;
            }
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

        // Build frustum planes for each cascade
        std::array<std::array<glm::vec4, 6>, CascadedShadowMap::kCascadeCount> cascadeFrustums;
        for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
            cascadeFrustums[i] = extractFrustumPlanes(csmMatrices[i]);
        }

        for (const auto& object : objects) {
            if (object.mesh) {
                bool inAnyCascade = false;
                for (int i = 0; i < CascadedShadowMap::kCascadeCount; ++i) {
                    if (isAabbInsideFrustum(object.mesh->aabbMin(), object.mesh->aabbMax(),
                                             object.modelMatrix, cascadeFrustums[i])) {
                        inAnyCascade = true;
                        break;
                    }
                }
                if (!inAnyCascade) {
                    ++shadowCulled;
                    continue;
                }
            }
            csmShader_->setMat4("uModel", object.modelMatrix);
            object.mesh->draw();
        }

        csmShadowMap_.unbind();
    }

    lastStats_.shadowCulledCount = shadowCulled;
}

void SceneRenderPipeline::renderScenePass(const SceneRenderInput& input,
                                           const std::vector<RenderLight>& lights,
                                           const ShadowRenderData& shadowData,
                                           int internalWidth,
                                           int internalHeight) {
    ensureFramebuffers(internalWidth, internalHeight, input.postParams);

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
    sceneShader_->setInt("uCsmShadowMap", TextureUnits::kCsmShadowMap);
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
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kCsmShadowMap);
    glBindTexture(GL_TEXTURE_2D_ARRAY, csmEnabled ? csmShadowMap_.depthArrayTexture() : 0);

    // Bind LTC lookup textures to units 10 and 11 (always real textures -- per Pitfall 1)
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kLtcMat);
    glBindTexture(GL_TEXTURE_2D, ltcData_.ltcMatTexture());
    sceneShader_->setInt("uLtcMat", TextureUnits::kLtcMat);
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kLtcAmp);
    glBindTexture(GL_TEXTURE_2D, ltcData_.ltcAmpTexture());
    sceneShader_->setInt("uLtcAmp", TextureUnits::kLtcAmp);

    const auto& reflectionSet = skyTextures_.resolvePrefilteredCube(input.postParams->sky.cubemapFacePaths);
    const bool hasEnvironmentCubemap = !input.postParams->sky.cubemapFacePaths[0].empty();
    const bool environmentReflectionsEnabled =
        hasEnvironmentCubemap &&
        reflectionSet.prefilteredSpecularCubemap != 0 &&
        skyTextures_.brdfLutTexture() != 0 &&
        input.postParams->sky.cubemapStrength > 0.0f;
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kEnvironmentSpecular);
    glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionSet.prefilteredSpecularCubemap);
    sceneShader_->setInt("uEnvironmentSpecularMap", TextureUnits::kEnvironmentSpecular);
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kEnvironmentBrdfLut);
    glBindTexture(GL_TEXTURE_2D, skyTextures_.brdfLutTexture());
    sceneShader_->setInt("uEnvironmentBrdfLut", TextureUnits::kEnvironmentBrdfLut);
    sceneShader_->setInt("uEnvironmentReflectionsEnabled", environmentReflectionsEnabled ? 1 : 0);
    sceneShader_->setInt("uEnvironmentSpecularMipCount", reflectionSet.specularMipCount);
    sceneShader_->setFloat("uEnvironmentReflectionStrength", input.postParams->sky.cubemapStrength);
    sceneShader_->setVec3("uEnvironmentReflectionTint", input.postParams->sky.cubemapTint);

    const RenderReflectionProbeState* reflectionProbe = input.reflectionProbe;
    const bool localProbeEnabled =
        reflectionProbe != nullptr &&
        reflectionProbe->enabled &&
        reflectionProbe->cubemap != 0 &&
        reflectionProbe->intensity > 0.0f;
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kReflectionProbeMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, localProbeEnabled ? reflectionProbe->cubemap : 0);
    sceneShader_->setInt("uReflectionProbeMap", TextureUnits::kReflectionProbeMap);
    sceneShader_->setInt("uReflectionProbeEnabled", localProbeEnabled ? 1 : 0);
    sceneShader_->setVec3("uReflectionProbeCenter", localProbeEnabled ? reflectionProbe->center : glm::vec3(0.0f));
    sceneShader_->setVec3("uReflectionProbeExtents", localProbeEnabled ? reflectionProbe->extents : glm::vec3(1.0f));
    sceneShader_->setFloat("uReflectionProbeBlendDistance", localProbeEnabled ? reflectionProbe->blendDistance : 0.0f);
    sceneShader_->setFloat("uReflectionProbeIntensity", localProbeEnabled ? reflectionProbe->intensity : 0.0f);
    sceneShader_->setInt("uReflectionProbeBoxProjection", localProbeEnabled ? reflectionProbe->boxProjection : 0);
    sceneShader_->setInt("uReflectionProbeMipCount", localProbeEnabled ? reflectionProbe->mipCount : 1);
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
    if (post.enableBloom) {
        bloomPass_.render(sceneFBO_.colorTexture(), post.bloomRadius * 0.003f, post.bloomThreshold);
    }
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
                          post.ssaoStrength,
                          post.ssaoFadeStart,
                          post.ssaoFadeEnd);
    }
    lastStats_.ssaoMs = (glfwGetTime() - tSsaoStart) * 1000.0;

    const double tCompositeStart = glfwGetTime();
    compositePass_.apply(sceneFBO_.colorTexture(),
                          sceneFBO_.depthTexture(),
                          sceneFBO_.normalTexture(),
                          post.enableBloom ? bloomPass_.bloomTexture() : 0,
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

void SceneRenderPipeline::ensureFramebuffers(int internalWidth,
                                             int internalHeight,
                                             const PostProcessParams* postParams) {
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
    const bool ssaoHalfResolution = (postParams == nullptr) ? true : postParams->ssaoHalfResolution;
    ssaoPass_.resize(ssaoHalfResolution ? std::max(safeW / 2, 1) : safeW,
                     ssaoHalfResolution ? std::max(safeH / 2, 1) : safeH);
}
