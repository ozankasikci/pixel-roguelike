#include "game/rendering/RuntimeSceneRenderer.h"

#include "engine/core/MathUtils.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/components/CameraComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/ReflectionProbeComponent.h"
#include "game/components/PivotTransformComponent.h"
#include "game/components/TransformComponent.h"
#include "game/components/ViewmodelComponent.h"
#include "game/modules/door/DoorMath.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/RuntimeCameraMath.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

// Positional constants kept as constexpr — not exposed to debug tuning.
constexpr float kPlayerTorchForwardOffset = 0.230f;
constexpr float kPlayerTorchRightOffset = -0.155f;
constexpr float kPlayerTorchDownOffset = 0.040f;
constexpr glm::vec3 kPlayerTorchSpillOffset{-0.030f, -0.180f, -0.060f};
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
    reflectionProbeRenderer_.init();
    materialTextureLibrary_.init(content);
}

void RuntimeSceneRenderer::shutdown() {
    reflectionProbeRenderer_.shutdown();
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

void RuntimeSceneRenderer::collectSceneObjects(entt::registry& registry,
                                               std::vector<RenderObject>& out) const {
    out.clear();
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

        glm::mat4 model;
        if (const auto* pivot = registry.try_get<PivotTransformComponent>(entity)) {
            model = makePivotLeafModel(transform.position, pivot->closedYawDeg,
                                        pivot->currentYawDeg, pivot->pivot,
                                        pivot->meshCenter, pivot->scale);
        } else {
            model = transform.modelMatrix();
        }
        out.push_back({
            mesh.mesh,
            model,
            mesh.tint,
            materialTextureLibrary_.resolve(mesh.materialId)
        });
    }
}

void RuntimeSceneRenderer::collectViewmodelObjects(entt::registry& registry,
                                                    const CameraState& camera,
                                                    float deltaTime,
                                                    std::vector<RenderObject>& out) const {
    out.clear();
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

        out.push_back({
            mesh.mesh,
            model,
            mesh.tint,
            materialTextureLibrary_.resolve(mesh.materialId)
        });
    }
}

void RuntimeSceneRenderer::collectLights(entt::registry& registry,
                                          const DebugParams& params,
                                          std::vector<RenderLight>& out) const {
    std::vector<RenderLight>& lights = out;
    lights.clear();

    auto cameraView = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : cameraView.each()) {
        if (!params.lighting.torch.enabled) {
            break;
        }
        const PlayerTorchOverride& torch = params.lighting.torch;
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
        torchSpill.color = torch.spillColor * (0.92f + visualFlicker * 0.14f);
        torchSpill.radius = torch.spillRadius * (0.95f + visualFlicker * 0.08f);
        torchSpill.intensity = torch.spillIntensity * torch.masterIntensity * (0.88f + visualFlicker * 0.22f);
        lights.push_back(torchSpill);

        RenderLight torchHalo;
        torchHalo.type = LightType::Point;
        torchHalo.position = transform.position + cameraUp * -0.22f;
        torchHalo.color = torch.haloColor * (0.92f + visualFlicker * 0.10f);
        torchHalo.radius = torch.haloRadius * (0.97f + visualFlicker * 0.05f);
        torchHalo.intensity = torch.haloIntensity * torch.masterIntensity * (0.92f + visualFlicker * 0.12f);
        lights.push_back(torchHalo);

        RenderLight torchLight;
        torchLight.type = LightType::Spot;
        torchLight.position = flamePosition;
        torchLight.direction = torchDirection;
        torchLight.color = torch.torchColor * (0.95f + lightFlicker * 0.04f);
        torchLight.radius = torch.torchRadius * (0.98f + lightFlicker * 0.05f);
        torchLight.intensity = torch.torchIntensity * torch.masterIntensity * lightFlicker;
        torchLight.innerConeDegrees = clampInnerCone(torch.torchInnerConeDegrees, torch.torchOuterConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, torch.torchOuterConeDegrees);
        torchLight.castsShadows = true;
        lights.push_back(torchLight);

        glm::vec3 handGlowPosition = transform.position
            + camera.forward * kPlayerHandGlowForwardOffset
            + camera.right * kPlayerHandGlowRightOffset
            + glm::vec3(0.0f, -kPlayerHandGlowDownOffset, 0.0f);
        RenderLight handGlow;
        handGlow.type = LightType::Point;
        handGlow.position = handGlowPosition;
        handGlow.color = torch.handGlowColor * (0.98f + visualFlicker * 0.03f);
        handGlow.radius = torch.handGlowRadius * (0.99f + lightFlicker * 0.03f);
        handGlow.intensity = torch.handGlowIntensity * torch.masterIntensity * (0.96f + lightFlicker * 0.05f);
        lights.push_back(handGlow);
        break;
    }

    appendDirectionalLight(lights, params.lighting.sunDirectional, glm::vec3(0.0f, -1.0f, 0.0f));
    appendDirectionalLight(lights, params.lighting.fillDirectional, glm::vec3(0.0f, -1.0f, 0.0f));

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
}

void RuntimeSceneRenderer::collectReflectionProbes(entt::registry& registry,
                                                    std::vector<RenderReflectionProbeInput>& out) const {
    out.clear();

    auto probeView = registry.view<TransformComponent, ReflectionProbeComponent>();
    for (auto [entity, transform, probe] : probeView.each()) {
        RenderReflectionProbeInput inputProbe;
        inputProbe.id = static_cast<std::uint64_t>(entt::to_integral(entity));
        inputProbe.center = transform.position;
        inputProbe.extents = probe.extents;
        inputProbe.blendDistance = probe.blendDistance;
        inputProbe.intensity = probe.intensity;
        inputProbe.boxProjection = probe.boxProjection;
        inputProbe.dirty = probe.dirty;
        out.push_back(inputProbe);
    }
}

void RuntimeSceneRenderer::updateDebugParams(DebugParams& params,
                                             const CameraState& camera,
                                             float deltaTime,
                                             std::size_t drawCalls) const {
    params.camera.position = camera.position;
    params.camera.direction = camera.direction;
    params.camera.fov = glm::degrees(2.0f * std::atan(1.0f / camera.projectionMatrix[1][1]));
    params.camera.moveSpeed = camera.moveSpeed;
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
    collectSceneObjects(registry, scene_objects_);
    collectViewmodelObjects(registry, camera, deltaTime, viewmodel_objects_);
    collectLights(registry, params, lights_);
    collectReflectionProbes(registry, reflection_probes_);

    // Build SceneRenderInput from ECS-collected data
    SceneRenderInput input;
    input.objects = &scene_objects_;
    input.viewmodelObjects = &viewmodel_objects_;
    input.lights = &lights_;
    input.viewMatrix = camera.viewMatrix;
    input.projectionMatrix = camera.projectionMatrix;
    input.cameraPosition = camera.position;
    input.nearPlane = camera.nearPlane;
    input.farPlane = camera.farPlane;
    input.postParams = &params.post;
    input.shadowsEnabled = params.lighting.shadowsEnabled;
    input.shadowResolutionIndex = params.lighting.shadowMapResolutionIndex;

    // Build LightingEnvironment from DebugParams
    input.lightingEnvironment.hemisphereSkyColor = params.lighting.hemisphereSkyColor;
    input.lightingEnvironment.hemisphereGroundColor = params.lighting.hemisphereGroundColor;
    input.lightingEnvironment.hemisphereStrength = params.lighting.hemisphereStrength;
    input.lightingEnvironment.enableDirectionalLights = params.lighting.enableDirectionalLights;
    input.lightingEnvironment.sun = params.lighting.sunDirectional;
    input.lightingEnvironment.sun.direction = safeNormalize(input.lightingEnvironment.sun.direction,
                                                             glm::vec3(0.0f, -1.0f, 0.0f));
    input.lightingEnvironment.fill = params.lighting.fillDirectional;
    input.lightingEnvironment.fill.direction = safeNormalize(input.lightingEnvironment.fill.direction,
                                                              glm::vec3(0.0f, -1.0f, 0.0f));
    input.lightingEnvironment.enableShadows = params.lighting.shadowsEnabled;
    input.lightingEnvironment.shadowBias = params.lighting.shadowBias;
    input.lightingEnvironment.shadowNormalBias = params.lighting.shadowNormalBias;

    const auto capturedProbeIds = reflectionProbeRenderer_.updateDirtyProbes(reflection_probes_, input, pipeline_);
    if (!capturedProbeIds.empty()) {
        auto probeView = registry.view<ReflectionProbeComponent>();
        for (auto [entity, probe] : probeView.each()) {
            const std::uint64_t probeId = static_cast<std::uint64_t>(entt::to_integral(entity));
            if (std::find(capturedProbeIds.begin(), capturedProbeIds.end(), probeId) != capturedProbeIds.end()) {
                probe.dirty = false;
            }
        }
    }

    active_reflection_probe_ = reflectionProbeRenderer_.selectNearestProbe(camera.position, reflection_probes_);
    input.reflectionProbe = active_reflection_probe_.enabled ? &active_reflection_probe_ : nullptr;

    pipeline_.render(input, internalWidth, internalHeight, outputWidth, outputHeight, targetFramebuffer);
    updateDebugParams(params, camera, deltaTime, scene_objects_.size());

    if (output != nullptr) {
        output->lights = lights_;
        output->drawCalls = scene_objects_.size();
    }
}
