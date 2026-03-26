#include "game/systems/RenderSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/components/TransformComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/ViewmodelComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InventoryMenuState.h"
#include "game/ui/InteractionPromptState.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

// constexpr definitions (needed in pre-C++17 out-of-line)
constexpr int RenderSystem::RES_W[];
constexpr int RenderSystem::RES_H[];

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

float playerTorchVisualFlicker(float timeSeconds) {
    float pulseA = std::sin(timeSeconds * 5.7f) * 0.5f + 0.5f;
    float pulseB = std::sin(timeSeconds * 11.9f + 1.7f) * 0.5f + 0.5f;
    float pulseC = std::sin(timeSeconds * 18.3f + 0.4f) * 0.5f + 0.5f;
    float shaped = pulseA * 0.50f + pulseB * 0.32f + pulseC * 0.18f;
    return 0.90f + shaped * 0.24f;
}

float playerTorchLightFlicker(float timeSeconds) {
    float drift = std::sin(timeSeconds * 2.2f + 0.7f) * 0.5f + 0.5f;
    float flutter = std::sin(timeSeconds * 7.4f + 2.1f) * 0.5f + 0.5f;
    float shaped = drift * 0.62f + flutter * 0.38f;
    return 0.92f + shaped * 0.16f;
}

float clampInnerCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(innerConeDegrees, 2.0f, std::max(outerConeDegrees - 1.0f, 3.0f));
}

float clampOuterCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(outerConeDegrees, innerConeDegrees + 1.0f, 85.0f);
}

} // namespace

void RenderSystem::init(Application& app) {
    sceneShader_ = std::make_unique<Shader>("assets/shaders/game/scene.vert", "assets/shaders/game/scene.frag");
    shadowShader_ = std::make_unique<Shader>("assets/shaders/engine/shadow_depth.vert", "assets/shaders/engine/shadow_depth.frag");
    renderer_    = std::make_unique<Renderer>(sceneShader_.get());
    sceneFBO_.create(RES_W[2], RES_H[2]);  // default 720p
    compositeFBO_.create(RES_W[2], RES_H[2]);
    for (auto& shadowMap : shadowMaps_) {
        shadowMap.create(shadowResolution());
    }
    imguiLayer_.init(app.window().handle());
}

RenderSystem::CameraState RenderSystem::captureCamera(entt::registry& registry) const {
    CameraState camera;

    auto camView = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, cam] : camView.each()) {
        camera.position = transform.position;
        camera.viewMatrix = cam.viewMatrix;
        camera.projectionMatrix = cam.projectionMatrix;
        camera.direction = cam.forward;
        break;
    }

    return camera;
}

std::vector<RenderObject> RenderSystem::collectSceneObjects(entt::registry& registry) const {
    std::vector<RenderObject> objects;
    MeshAssetProvider* provider = registry.ctx().contains<MeshAssetProvider>()
        ? &registry.ctx().get<MeshAssetProvider>()
        : nullptr;

    auto meshView = registry.view<TransformComponent, MeshComponent>();
    for (auto [entity, transform, mesh] : meshView.each()) {
        if (mesh.mesh == nullptr && provider != nullptr && provider->library != nullptr && !mesh.meshId.empty()) {
            mesh.mesh = provider->library->get(mesh.meshId);
        }
        if (mesh.mesh == nullptr) continue;
        if (registry.any_of<ViewmodelComponent>(entity)) continue;
        glm::mat4 model = mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix();
        objects.push_back({mesh.mesh, model, mesh.tint, mesh.material});
    }

    return objects;
}

std::vector<RenderObject> RenderSystem::collectViewmodelObjects(entt::registry& registry,
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
        if (mesh.mesh == nullptr) continue;

        vm.bobTime += deltaTime;
        float bobOffset = std::sin(vm.bobTime * vm.bobSpeed * 6.2831853f) * vm.bobAmplitude;
        glm::vec3 offset = vm.viewOffset + glm::vec3(0.0f, bobOffset, 0.0f);

        glm::mat4 invView = glm::inverse(camera.viewMatrix);
        glm::vec3 worldPos = glm::vec3(invView * glm::vec4(offset, 1.0f));

        glm::mat4 model = glm::mat4(glm::mat3(invView));
        model[3] = glm::vec4(worldPos, 1.0f);

        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = model * glm::rotate(glm::mat4(1.0f), glm::radians(vm.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = model * glm::scale(glm::mat4(1.0f), vm.scale);
        model = model * glm::translate(glm::mat4(1.0f), -vm.meshCenter);

        objects.push_back({mesh.mesh, model, mesh.tint, mesh.material});
    }

    return objects;
}

std::vector<RenderLight> RenderSystem::collectLights(entt::registry& registry) const {
    std::vector<RenderLight> lights;

    auto cameraView = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : cameraView.each()) {
        const float timeSeconds = static_cast<float>(glfwGetTime());
        const float visualFlicker = playerTorchVisualFlicker(timeSeconds);
        const float lightFlicker = playerTorchLightFlicker(timeSeconds);
        const glm::vec3 cameraUp = safeNormalize(glm::cross(camera.right, camera.forward), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 torchDirection = safeNormalize(
            camera.forward * 0.14f
                + camera.right * 0.03f
                + cameraUp * -0.48f,
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

        glm::vec3 torchColor = kPlayerTorchColor * (0.95f + lightFlicker * 0.04f);
        RenderLight torchLight;
        torchLight.type = LightType::Spot;
        torchLight.position = flamePosition;
        torchLight.direction = torchDirection;
        torchLight.color = torchColor;
        torchLight.radius = kPlayerTorchRadius * (0.98f + lightFlicker * 0.05f);
        torchLight.intensity = kPlayerTorchIntensity * lightFlicker;
        torchLight.innerConeDegrees = clampInnerCone(debugParams_.playerTorchInnerConeDegrees, debugParams_.playerTorchOuterConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, debugParams_.playerTorchOuterConeDegrees);
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

void RenderSystem::assignShadowSlots(std::vector<RenderLight>& lights) const {
    for (auto& light : lights) {
        light.shadowIndex = -1;
    }

    if (!debugParams_.shadowsEnabled) {
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

LightingEnvironment RenderSystem::lightingEnvironment() const {
    LightingEnvironment lighting;
    lighting.hemisphereSkyColor = debugParams_.hemisphereSkyColor;
    lighting.hemisphereGroundColor = debugParams_.hemisphereGroundColor;
    lighting.hemisphereStrength = debugParams_.hemisphereStrength;
    lighting.enableDirectionalLights = debugParams_.enableDirectionalLights;
    lighting.directionalIntensityScale = debugParams_.directionalLightIntensityScale;
    lighting.enableShadows = debugParams_.shadowsEnabled;
    lighting.shadowBias = debugParams_.shadowBias;
    lighting.shadowNormalBias = debugParams_.shadowNormalBias;
    return lighting;
}

int RenderSystem::shadowResolution() const {
    const int clampedIndex = std::clamp(debugParams_.shadowMapResolutionIndex, 0, 2);
    return kShadowResolutions[clampedIndex];
}

glm::mat4 RenderSystem::buildShadowMatrix(const RenderLight& light) const {
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

void RenderSystem::renderShadowPass(const std::vector<RenderObject>& objects,
                                    const std::vector<RenderLight>& lights,
                                    ShadowRenderData& shadowData) {
    shadowData.shadowCount = 0;
    shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
    shadowData.textures = {
        shadowMaps_[0].texture(),
        shadowMaps_[1].texture(),
    };

    if (!debugParams_.shadowsEnabled || shadowShader_ == nullptr) {
        return;
    }

    const int desiredResolution = shadowResolution();
    for (auto& shadowMap : shadowMaps_) {
        shadowMap.resize(desiredResolution);
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

void RenderSystem::renderScenePass(const CameraState& camera,
                                   const std::vector<RenderObject>& objects,
                                   const std::vector<RenderObject>& viewmodelObjects,
                                   const std::vector<RenderLight>& lights) {
    ShadowRenderData shadowData;
    renderShadowPass(objects, lights, shadowData);

    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    const LightingEnvironment lighting = lightingEnvironment();
    renderer_->drawScene(objects,
                         lights,
                         lighting,
                         shadowData,
                         camera.viewMatrix,
                         camera.projectionMatrix,
                         camera.position);

    if (!viewmodelObjects.empty()) {
        glDepthRange(0.0, 0.01);
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

void RenderSystem::renderPostProcess(Application& app, const CameraState& camera) {
    int displayW = app.window().width();
    int displayH = app.window().height();
    glClear(GL_COLOR_BUFFER_BIT);

    (void)camera;
    debugParams_.post.nearPlane = 0.1f;
    debugParams_.post.farPlane  = 100.0f;
    debugParams_.post.timeSeconds = static_cast<float>(glfwGetTime());

    compositePass_.apply(sceneFBO_.colorTexture(),
                         sceneFBO_.depthTexture(),
                         sceneFBO_.normalTexture(),
                         compositeFBO_.framebuffer(),
                         debugParams_.post,
                         compositeFBO_.width(),
                         compositeFBO_.height());

    stylizePass_.apply(compositeFBO_.colorTexture(),
                       sceneFBO_.colorTexture(),
                       sceneFBO_.depthTexture(),
                       sceneFBO_.normalTexture(),
                       debugParams_.post,
                       displayW, displayH);
}

InteractionPromptState& RenderSystem::ensurePromptState(entt::registry& registry) const {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    return ctx.get<InteractionPromptState>();
}

void RenderSystem::updateDebugParams(const CameraState& camera, float deltaTime, std::size_t drawCalls) {
    debugParams_.cameraPos   = camera.position;
    debugParams_.cameraDir   = camera.direction;
    debugParams_.fps         = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
    debugParams_.frameTimeMs = deltaTime * 1000.0f;
    debugParams_.drawCalls   = static_cast<int>(drawCalls);
}

void RenderSystem::renderOverlays(Application& app,
                                  entt::registry& registry,
                                  std::vector<RenderLight>& lights,
                                  InteractionPromptState& prompt) {
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    if (!overlaysVisible_ && !prompt.visible && !inventoryOpen) {
        return;
    }

    imguiLayer_.beginFrame();

    if (inventoryOpen) {
        auto& menu = registry.ctx().get<InventoryMenuState>();
        const auto& session = app.getService<RunSession>();
        const auto& content = app.getService<ContentRegistry>();
        const auto equipment = resolveEffectiveEquipment(session, content);
        ImGuiLayer::renderInventory(menu, session, content, equipment);
    }

    if (overlaysVisible_) {
        ImGuiLayer::renderOverlay(debugParams_, lights);

        auto movView = registry.view<PlayerMovementComponent>();
        for (auto [entity, movement] : movView.each()) {
            ImGuiLayer::renderMovementOverlay(movement, movement.grounded);
            break;
        }

        auto vmView = registry.view<MeshComponent, ViewmodelComponent>();
        for (auto [entity, mesh, vm] : vmView.each()) {
            ImGuiLayer::renderViewmodelOverlay(vm);
            break;
        }
    }

    if (prompt.visible && !inventoryOpen) {
        ImGuiLayer::renderInteractionPrompt(prompt.text.c_str(), prompt.busy);
    }

    imguiLayer_.endFrame();
}

void RenderSystem::handleResolutionChange() {
    if (!debugParams_.resolutionChanged) {
        return;
    }

    int idx = debugParams_.internalResIndex;
    sceneFBO_.resize(RES_W[idx], RES_H[idx]);
    compositeFBO_.resize(RES_W[idx], RES_H[idx]);
    spdlog::info("Internal resolution changed to {}x{}", RES_W[idx], RES_H[idx]);
    debugParams_.resolutionChanged = false;
}

void RenderSystem::handleCapture(Application& app, int displayW, int displayH) {
    GLFWwindow* win = app.window().handle();
    if (glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS && !f12Pressed_) {
        f12Pressed_ = true;
        saveScreenshot(displayW, displayH);
    }
    if (glfwGetKey(win, GLFW_KEY_F12) == GLFW_RELEASE) {
        f12Pressed_ = false;
    }

    if (autoCapture_.tick(displayW, displayH)) {
        spdlog::info("Auto-screenshot captured, exiting");
        app.requestQuit();
    }
}

void RenderSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();
    const bool escapeOpenedCursor = input_.isKeyJustPressed(GLFW_KEY_ESCAPE) && !input_.isCursorLocked();
    CameraState camera = captureCamera(registry);
    std::vector<RenderObject> objects = collectSceneObjects(registry);
    std::vector<RenderObject> viewmodelObjects = collectViewmodelObjects(registry, camera, deltaTime);
    std::vector<RenderLight> lights = collectLights(registry);
    assignShadowSlots(lights);

    renderScenePass(camera, objects, viewmodelObjects, lights);
    renderPostProcess(app, camera);

    GLFWwindow* win = app.window().handle();
    if (glfwGetKey(win, GLFW_KEY_F1) == GLFW_PRESS && !f1Pressed_) {
        f1Pressed_ = true;
        overlaysVisible_ = !overlaysVisible_;
    }
    if (glfwGetKey(win, GLFW_KEY_F1) == GLFW_RELEASE) {
        f1Pressed_ = false;
    }

    if (escapeOpenedCursor) {
        const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
            && registry.ctx().get<InventoryMenuState>().open;
        if (!inventoryOpen) {
            overlaysVisible_ = true;
        }
    }

    updateDebugParams(camera, deltaTime, objects.size());
    auto& prompt = ensurePromptState(registry);
    renderOverlays(app, registry, lights, prompt);
    handleResolutionChange();
    handleCapture(app, app.window().width(), app.window().height());
}

void RenderSystem::shutdown() {
    imguiLayer_.shutdown();
}

void RenderSystem::enableAutoScreenshot(const std::string& path, int delayFrames) {
    autoCapture_.enable(path, delayFrames);
    spdlog::info("Auto-screenshot enabled: {}", path);
}
