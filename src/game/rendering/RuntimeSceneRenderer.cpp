#include "game/rendering/RuntimeSceneRenderer.h"

#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/components/CameraComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/components/ViewmodelComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/RuntimeCameraMath.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

constexpr glm::vec3 kPlayerTorchColor{1.00f, 0.89f, 0.76f};
constexpr glm::vec3 kPlayerTorchSpillColor{1.00f, 0.78f, 0.46f};
constexpr float kPlayerTorchRadius = 3.1f;
constexpr float kPlayerTorchIntensity = 0.07f;
constexpr float kPlayerTorchForwardOffset = 0.230f;
constexpr float kPlayerTorchRightOffset = -0.155f;
constexpr float kPlayerTorchDownOffset = 0.040f;
constexpr glm::vec3 kPlayerTorchSpillOffset{-0.030f, -0.180f, -0.060f};
constexpr float kPlayerTorchSpillRadius = 7.2f;
constexpr float kPlayerTorchSpillIntensity = 1.12f;
constexpr float kPlayerTorchHaloRadius = 5.4f;
constexpr float kPlayerTorchHaloIntensity = 0.54f;
constexpr glm::vec3 kPlayerTorchHaloColor{1.00f, 0.84f, 0.58f};
constexpr glm::vec3 kPlayerHandGlowColor{0.98f, 0.91f, 0.82f};
constexpr float kPlayerHandGlowRadius = 1.10f;
constexpr float kPlayerHandGlowIntensity = 0.04f;
constexpr float kPlayerHandGlowForwardOffset = 0.08f;
constexpr float kPlayerHandGlowRightOffset = -0.14f;
constexpr float kPlayerHandGlowDownOffset = 0.06f;
constexpr int kShadowResolutions[] = {512, 1024, 2048};

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

void appendDirectionalLight(std::vector<RenderLight>& lights,
                            const DirectionalLightSlot& slot,
                            const glm::vec3& fallbackDirection) {
    if (!slot.enabled || slot.intensity <= 0.0001f) {
        return;
    }

    RenderLight renderLight;
    renderLight.type = LightType::Directional;
    renderLight.direction = safeNormalize(slot.direction, fallbackDirection);
    renderLight.color = glm::max(slot.color, glm::vec3(0.0f));
    renderLight.intensity = slot.intensity;
    lights.push_back(renderLight);
}

float playerTorchVisualFlicker(float timeSeconds) {
    const float pulseA = std::sin(timeSeconds * 5.7f) * 0.5f + 0.5f;
    const float pulseB = std::sin(timeSeconds * 11.9f + 1.7f) * 0.5f + 0.5f;
    const float pulseC = std::sin(timeSeconds * 18.3f + 0.4f) * 0.5f + 0.5f;
    const float shaped = pulseA * 0.50f + pulseB * 0.32f + pulseC * 0.18f;
    return 0.90f + shaped * 0.24f;
}

float playerTorchLightFlicker(float timeSeconds) {
    const float drift = std::sin(timeSeconds * 2.2f + 0.7f) * 0.5f + 0.5f;
    const float flutter = std::sin(timeSeconds * 7.4f + 2.1f) * 0.5f + 0.5f;
    const float shaped = drift * 0.62f + flutter * 0.38f;
    return 0.92f + shaped * 0.16f;
}

float clampInnerCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(innerConeDegrees, 2.0f, std::max(outerConeDegrees - 1.0f, 3.0f));
}

float clampOuterCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(outerConeDegrees, innerConeDegrees + 1.0f, 85.0f);
}

} // namespace

void RuntimeSceneRenderer::init(const ContentRegistry& content) {
    sceneShader_ = std::make_unique<Shader>("assets/shaders/game/scene.vert", "assets/shaders/game/scene.frag");
    shadowShader_ = std::make_unique<Shader>("assets/shaders/engine/shadow_depth.vert", "assets/shaders/engine/shadow_depth.frag");
    renderer_ = std::make_unique<Renderer>(sceneShader_.get());
    materialTextureLibrary_.init(content);
    ensureFramebuffers(1280, 720);
}

void RuntimeSceneRenderer::shutdown() {
    renderer_.reset();
    shadowShader_.reset();
    sceneShader_.reset();
}

void RuntimeSceneRenderer::reloadContent(const ContentRegistry& content) {
    materialTextureLibrary_.init(content);
}

std::size_t RuntimeSceneRenderer::prewarmMaterialResources(entt::registry& registry) {
    std::unordered_set<std::string> warmedMaterials;
    auto meshView = registry.view<MeshComponent>();
    for (auto [entity, mesh] : meshView.each()) {
        (void)entity;
        (void)materialTextureLibrary_.resolve(mesh.materialId, mesh.material);
        const std::string key = mesh.materialId.empty()
            ? std::to_string(static_cast<int>(mesh.material))
            : mesh.materialId;
        warmedMaterials.insert(key);
    }
    return warmedMaterials.size();
}

RuntimeSceneRenderer::CameraState RuntimeSceneRenderer::captureCamera(entt::registry& registry, float aspect) const {
    const RuntimeCameraState captured = capturePrimaryRuntimeCamera(registry, aspect);
    return CameraState{
        captured.position,
        captured.viewMatrix,
        captured.projectionMatrix,
        captured.direction,
        captured.nearPlane,
        captured.farPlane,
        captured.moveSpeed,
    };
}

std::vector<RenderObject> RuntimeSceneRenderer::collectSceneObjects(entt::registry& registry) const {
    std::vector<RenderObject> objects;
    MeshAssetProvider* provider = registry.ctx().contains<MeshAssetProvider>()
        ? &registry.ctx().get<MeshAssetProvider>()
        : nullptr;

    auto meshView = registry.view<TransformComponent, MeshComponent>();
    for (auto [entity, transform, mesh] : meshView.each()) {
        if (mesh.mesh == nullptr && provider != nullptr && provider->library != nullptr && !mesh.meshId.empty()) {
            mesh.mesh = provider->library->get(mesh.meshId);
        }
        if (mesh.mesh == nullptr || registry.any_of<ViewmodelComponent>(entity)) {
            continue;
        }

        const glm::mat4 model = mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix();
        objects.push_back({
            mesh.mesh,
            model,
            mesh.tint,
            materialTextureLibrary_.resolve(mesh.materialId, mesh.material)
        });
    }

    return objects;
}

std::vector<RenderObject> RuntimeSceneRenderer::collectViewmodelObjects(entt::registry& registry,
                                                                        const CameraState& camera,
                                                                        float deltaTime) const {
    std::vector<RenderObject> objects;
    MeshAssetProvider* provider = registry.ctx().contains<MeshAssetProvider>()
        ? &registry.ctx().get<MeshAssetProvider>()
        : nullptr;

    auto vmView = registry.view<MeshComponent, ViewmodelComponent>();
    for (auto [entity, mesh, vm] : vmView.each()) {
        if (mesh.mesh == nullptr && provider != nullptr && provider->library != nullptr && !mesh.meshId.empty()) {
            mesh.mesh = provider->library->get(mesh.meshId);
        }
        if (mesh.mesh == nullptr) {
            continue;
        }

        vm.bobTime += deltaTime;
        const float bobOffset = std::sin(vm.bobTime * vm.bobSpeed * 6.2831853f) * vm.bobAmplitude;
        const glm::vec3 offset = vm.viewOffset + glm::vec3(0.0f, bobOffset, 0.0f);

        const glm::mat4 invView = glm::inverse(camera.viewMatrix);
        const glm::vec3 worldPos = glm::vec3(invView * glm::vec4(offset, 1.0f));

        glm::mat4 model = glm::mat4(glm::mat3(invView));
        model[3] = glm::vec4(worldPos, 1.0f);
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = model * glm::scale(glm::mat4(1.0f), vm.scale);
        model = model * glm::translate(glm::mat4(1.0f), -vm.meshCenter);

        objects.push_back({
            mesh.mesh,
            model,
            mesh.tint,
            materialTextureLibrary_.resolve(mesh.materialId, mesh.material)
        });
    }

    return objects;
}

std::vector<RenderLight> RuntimeSceneRenderer::collectLights(entt::registry& registry,
                                                             const DebugParams& params) const {
    std::vector<RenderLight> lights;

    auto cameraView = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : cameraView.each()) {
        const float timeSeconds = static_cast<float>(glfwGetTime());
        const float visualFlicker = playerTorchVisualFlicker(timeSeconds);
        const float lightFlicker = playerTorchLightFlicker(timeSeconds);
        const glm::vec3 cameraUp = safeNormalize(glm::cross(camera.right, camera.forward), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 torchDirection = safeNormalize(
            camera.forward * 0.14f + camera.right * 0.03f + cameraUp * -0.48f,
            glm::vec3(0.0f, 0.0f, -1.0f)
        );
        glm::vec3 flamePosition = transform.position
            + camera.forward * kPlayerTorchForwardOffset
            + camera.right * kPlayerTorchRightOffset
            + cameraUp * -kPlayerTorchDownOffset;
        flamePosition += camera.right * (std::sin(timeSeconds * 6.3f) * 0.010f)
            + cameraUp * (std::sin(timeSeconds * 8.7f + 0.8f) * 0.012f);

        glm::vec3 spillPosition = transform.position
            + camera.right * kPlayerTorchSpillOffset.x
            + cameraUp * kPlayerTorchSpillOffset.y
            + camera.forward * kPlayerTorchSpillOffset.z;

        RenderLight torchSpill;
        torchSpill.type = LightType::Point;
        torchSpill.position = spillPosition;
        torchSpill.color = kPlayerTorchSpillColor * (0.92f + visualFlicker * 0.14f);
        torchSpill.radius = kPlayerTorchSpillRadius * (0.95f + visualFlicker * 0.08f);
        torchSpill.intensity = kPlayerTorchSpillIntensity * (0.88f + visualFlicker * 0.22f);
        lights.push_back(torchSpill);

        RenderLight torchHalo;
        torchHalo.type = LightType::Point;
        torchHalo.position = transform.position + cameraUp * -0.22f;
        torchHalo.color = kPlayerTorchHaloColor * (0.92f + visualFlicker * 0.10f);
        torchHalo.radius = kPlayerTorchHaloRadius * (0.97f + visualFlicker * 0.05f);
        torchHalo.intensity = kPlayerTorchHaloIntensity * (0.92f + visualFlicker * 0.12f);
        lights.push_back(torchHalo);

        RenderLight torchLight;
        torchLight.type = LightType::Spot;
        torchLight.position = flamePosition;
        torchLight.direction = torchDirection;
        torchLight.color = kPlayerTorchColor * (0.95f + lightFlicker * 0.04f);
        torchLight.radius = kPlayerTorchRadius * (0.98f + lightFlicker * 0.05f);
        torchLight.intensity = kPlayerTorchIntensity * lightFlicker;
        torchLight.innerConeDegrees = clampInnerCone(params.playerTorchInnerConeDegrees, params.playerTorchOuterConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, params.playerTorchOuterConeDegrees);
        torchLight.castsShadows = true;
        lights.push_back(torchLight);

        glm::vec3 handGlowPosition = transform.position
            + camera.forward * kPlayerHandGlowForwardOffset
            + camera.right * kPlayerHandGlowRightOffset
            + glm::vec3(0.0f, -kPlayerHandGlowDownOffset, 0.0f);
        RenderLight handGlow;
        handGlow.type = LightType::Point;
        handGlow.position = handGlowPosition;
        handGlow.color = kPlayerHandGlowColor * (0.98f + visualFlicker * 0.03f);
        handGlow.radius = kPlayerHandGlowRadius * (0.99f + lightFlicker * 0.03f);
        handGlow.intensity = kPlayerHandGlowIntensity * (0.96f + lightFlicker * 0.05f);
        lights.push_back(handGlow);
        break;
    }

    appendDirectionalLight(lights, params.sunDirectional, glm::vec3(0.0f, -1.0f, 0.0f));
    appendDirectionalLight(lights, params.fillDirectional, glm::vec3(0.0f, -1.0f, 0.0f));

    auto lightView = registry.view<TransformComponent, LightComponent>();
    for (auto [entity, transform, light] : lightView.each()) {
        RenderLight renderLight;
        renderLight.type = light.type;
        renderLight.position = transform.position;
        renderLight.direction = safeNormalize(light.direction, glm::vec3(0.0f, -1.0f, 0.0f));
        renderLight.color = light.color;
        renderLight.radius = light.radius;
        renderLight.intensity = light.intensity;
        renderLight.innerConeDegrees = clampInnerCone(light.innerConeDegrees, light.outerConeDegrees);
        renderLight.outerConeDegrees = clampOuterCone(renderLight.innerConeDegrees, light.outerConeDegrees);
        renderLight.castsShadows = light.type == LightType::Spot && light.castsShadows;
        lights.push_back(renderLight);
    }

    return lights;
}

void RuntimeSceneRenderer::assignShadowSlots(std::vector<RenderLight>& lights,
                                             const DebugParams& params) const {
    for (auto& light : lights) {
        light.shadowIndex = -1;
    }
    if (!params.shadowsEnabled) {
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

LightingEnvironment RuntimeSceneRenderer::lightingEnvironment(const DebugParams& params) const {
    LightingEnvironment lighting;
    lighting.hemisphereSkyColor = params.hemisphereSkyColor;
    lighting.hemisphereGroundColor = params.hemisphereGroundColor;
    lighting.hemisphereStrength = params.hemisphereStrength;
    lighting.enableDirectionalLights = params.enableDirectionalLights;
    lighting.sun = params.sunDirectional;
    lighting.sun.direction = safeNormalize(lighting.sun.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    lighting.fill = params.fillDirectional;
    lighting.fill.direction = safeNormalize(lighting.fill.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    lighting.enableShadows = params.shadowsEnabled;
    lighting.shadowBias = params.shadowBias;
    lighting.shadowNormalBias = params.shadowNormalBias;
    return lighting;
}

int RuntimeSceneRenderer::shadowResolution(const DebugParams& params) const {
    return kShadowResolutions[std::clamp(params.shadowMapResolutionIndex, 0, 2)];
}

glm::mat4 RuntimeSceneRenderer::buildShadowMatrix(const RenderLight& light) const {
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

void RuntimeSceneRenderer::renderShadowPass(const std::vector<RenderObject>& objects,
                                            const std::vector<RenderLight>& lights,
                                            const DebugParams& params,
                                            ShadowRenderData& shadowData) {
    shadowData.shadowCount = 0;
    shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
    shadowData.textures = {shadowMaps_[0].texture(), shadowMaps_[1].texture()};

    if (!params.shadowsEnabled || shadowShader_ == nullptr) {
        return;
    }

    const int desiredResolution = shadowResolution(params);
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
}

void RuntimeSceneRenderer::renderScenePass(const CameraState& camera,
                                           const std::vector<RenderObject>& objects,
                                           const std::vector<RenderObject>& viewmodelObjects,
                                           const std::vector<RenderLight>& lights,
                                           const DebugParams& params,
                                           int internalWidth,
                                           int internalHeight) {
    ensureFramebuffers(internalWidth, internalHeight);

    ShadowRenderData shadowData;
    renderShadowPass(objects, lights, params, shadowData);

    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    const LightingEnvironment lighting = lightingEnvironment(params);
    const float timeSeconds = static_cast<float>(glfwGetTime());
    sceneShader_->use();
    sceneShader_->setFloat("uTimeSeconds", timeSeconds);
    renderer_->drawScene(objects,
                         lights,
                         lighting,
                         shadowData,
                         camera.viewMatrix,
                         camera.projectionMatrix,
                         camera.position);

    if (!viewmodelObjects.empty()) {
        glDepthRange(0.0, 0.01);
        sceneShader_->use();
        sceneShader_->setFloat("uTimeSeconds", timeSeconds);
        renderer_->drawScene(viewmodelObjects,
                             lights,
                             lighting,
                             shadowData,
                             camera.viewMatrix,
                             camera.projectionMatrix,
                             camera.position);
        glDepthRange(0.0, 1.0);
    }

    sceneFBO_.unbind();
    glDisable(GL_DEPTH_TEST);
}

void RuntimeSceneRenderer::renderPostProcess(const CameraState& camera,
                                             DebugParams& params,
                                             int outputWidth,
                                             int outputHeight,
                                             GLuint targetFramebuffer) {
    params.post.nearPlane = camera.nearPlane;
    params.post.farPlane = camera.farPlane;
    params.post.timeSeconds = static_cast<float>(glfwGetTime());
    params.post.inverseViewProjection = glm::inverse(camera.projectionMatrix * camera.viewMatrix);
    syncSkySunFromDirectional(params);

    compositePass_.apply(sceneFBO_.colorTexture(),
                         sceneFBO_.depthTexture(),
                         sceneFBO_.normalTexture(),
                         compositeFBO_.framebuffer(),
                         params.post,
                         compositeFBO_.width(),
                         compositeFBO_.height());

    stylizePass_.apply(compositeFBO_.colorTexture(),
                       sceneFBO_.colorTexture(),
                       sceneFBO_.depthTexture(),
                       sceneFBO_.normalTexture(),
                       params.post,
                       outputWidth,
                       outputHeight,
                       targetFramebuffer);
}

void RuntimeSceneRenderer::updateDebugParams(DebugParams& params,
                                             const CameraState& camera,
                                             float deltaTime,
                                             std::size_t drawCalls) const {
    params.cameraPos = camera.position;
    params.cameraDir = camera.direction;
    params.cameraFov = glm::degrees(2.0f * std::atan(1.0f / camera.projectionMatrix[1][1]));
    params.cameraSpeed = camera.moveSpeed;
    params.fps = deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f;
    params.frameTimeMs = deltaTime * 1000.0f;
    params.drawCalls = static_cast<int>(drawCalls);
}

void RuntimeSceneRenderer::ensureFramebuffers(int internalWidth, int internalHeight) {
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
}

void RuntimeSceneRenderer::render(entt::registry& registry,
                                  DebugParams& params,
                                  float deltaTime,
                                  int internalWidth,
                                  int internalHeight,
                                  int outputWidth,
                                  int outputHeight,
                                  GLuint targetFramebuffer,
                                  RuntimeEnvironmentSyncState* environmentState,
                                  bool preserveEnvironmentOverrides,
                                  RuntimeSceneRenderOutput* output) {
    syncEnvironmentFromRegistry(registry, params, environmentState, preserveEnvironmentOverrides);

    const float aspect = static_cast<float>(std::max(internalWidth, 1)) / static_cast<float>(std::max(internalHeight, 1));
    const CameraState camera = captureCamera(registry, aspect);
    std::vector<RenderObject> objects = collectSceneObjects(registry);
    std::vector<RenderObject> viewmodelObjects = collectViewmodelObjects(registry, camera, deltaTime);
    std::vector<RenderLight> lights = collectLights(registry, params);
    assignShadowSlots(lights, params);

    renderScenePass(camera, objects, viewmodelObjects, lights, params, internalWidth, internalHeight);
    renderPostProcess(camera, params, outputWidth, outputHeight, targetFramebuffer);
    updateDebugParams(params, camera, deltaTime, objects.size());

    if (output != nullptr) {
        output->lights = std::move(lights);
        output->drawCalls = objects.size();
    }
}
