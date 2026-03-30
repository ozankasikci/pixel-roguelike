#include "game/rendering/RuntimeSceneRenderer.h"

#include "engine/core/MathUtils.h"
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
constexpr float kPlayerTorchIntensity = 0.15f;
constexpr float kPlayerTorchForwardOffset = 0.230f;
constexpr float kPlayerTorchRightOffset = -0.155f;
constexpr float kPlayerTorchDownOffset = 0.040f;
constexpr glm::vec3 kPlayerTorchSpillOffset{-0.030f, -0.180f, -0.060f};
constexpr float kPlayerTorchSpillRadius = 7.2f;
constexpr float kPlayerTorchSpillIntensity = 2.5f;
constexpr float kPlayerTorchHaloRadius = 5.4f;
constexpr float kPlayerTorchHaloIntensity = 1.2f;
constexpr glm::vec3 kPlayerTorchHaloColor{1.00f, 0.84f, 0.58f};
constexpr float kPlayerTorchInnerConeDegrees = 58.0f;
constexpr float kPlayerTorchOuterConeDegrees = 82.0f;
constexpr glm::vec3 kPlayerHandGlowColor{0.98f, 0.91f, 0.82f};
constexpr float kPlayerHandGlowRadius = 1.10f;
constexpr float kPlayerHandGlowIntensity = 0.08f;
constexpr float kPlayerHandGlowForwardOffset = 0.08f;
constexpr float kPlayerHandGlowRightOffset = -0.14f;
constexpr float kPlayerHandGlowDownOffset = 0.06f;

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
    pipeline_.init();
    materialTextureLibrary_.init(content);
}

void RuntimeSceneRenderer::shutdown() {
    pipeline_.shutdown();
}

void RuntimeSceneRenderer::reloadContent(const ContentRegistry& content) {
    materialTextureLibrary_.init(content);
}

std::size_t RuntimeSceneRenderer::prewarmMaterialResources(entt::registry& registry) {
    std::unordered_set<std::string> warmedMaterials;
    auto meshView = registry.view<MeshComponent>();
    for (auto [entity, mesh] : meshView.each()) {
        (void)entity;
        (void)materialTextureLibrary_.resolve(mesh.materialId);
        const std::string key = mesh.materialId.empty() ? "stone_default" : mesh.materialId;
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
            materialTextureLibrary_.resolve(mesh.materialId)
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
            materialTextureLibrary_.resolve(mesh.materialId)
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
        torchLight.innerConeDegrees = clampInnerCone(kPlayerTorchInnerConeDegrees, kPlayerTorchOuterConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, kPlayerTorchOuterConeDegrees);
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
        if (light.type == LightType::AreaRect || light.type == LightType::Tube) {
            renderLight.right = light.right;
            renderLight.up = light.up;
            renderLight.width = light.width;
            renderLight.height = light.height;
            renderLight.doubleSided = light.doubleSided;
        }
        lights.push_back(renderLight);
    }

    return lights;
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

    // Build SceneRenderInput from ECS-collected data
    SceneRenderInput input;
    input.objects = &objects;
    input.viewmodelObjects = &viewmodelObjects;
    input.lights = &lights;
    input.viewMatrix = camera.viewMatrix;
    input.projectionMatrix = camera.projectionMatrix;
    input.cameraPosition = camera.position;
    input.nearPlane = camera.nearPlane;
    input.farPlane = camera.farPlane;
    input.postParams = &params.post;
    input.shadowsEnabled = params.shadowsEnabled;
    input.shadowResolutionIndex = params.shadowMapResolutionIndex;

    // Build LightingEnvironment from DebugParams
    input.lightingEnvironment.hemisphereSkyColor = params.hemisphereSkyColor;
    input.lightingEnvironment.hemisphereGroundColor = params.hemisphereGroundColor;
    input.lightingEnvironment.hemisphereStrength = params.hemisphereStrength;
    input.lightingEnvironment.enableDirectionalLights = params.enableDirectionalLights;
    input.lightingEnvironment.sun = params.sunDirectional;
    input.lightingEnvironment.sun.direction = safeNormalize(input.lightingEnvironment.sun.direction,
                                                             glm::vec3(0.0f, -1.0f, 0.0f));
    input.lightingEnvironment.fill = params.fillDirectional;
    input.lightingEnvironment.fill.direction = safeNormalize(input.lightingEnvironment.fill.direction,
                                                              glm::vec3(0.0f, -1.0f, 0.0f));
    input.lightingEnvironment.enableShadows = params.shadowsEnabled;
    input.lightingEnvironment.shadowBias = params.shadowBias;
    input.lightingEnvironment.shadowNormalBias = params.shadowNormalBias;

    pipeline_.render(input, internalWidth, internalHeight, outputWidth, outputHeight, targetFramebuffer);
    updateDebugParams(params, camera, deltaTime, objects.size());

    if (output != nullptr) {
        output->lights = std::move(lights);
        output->drawCalls = objects.size();
    }
}
