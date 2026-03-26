#include "game/systems/RenderSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include "engine/rendering/MeshLibrary.h"
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
#include <cmath>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

// constexpr definitions (needed in pre-C++17 out-of-line)
constexpr int RenderSystem::RES_W[];
constexpr int RenderSystem::RES_H[];

void RenderSystem::init(Application& app) {
    sceneShader_ = std::make_unique<Shader>("assets/shaders/scene.vert", "assets/shaders/scene.frag");
    renderer_    = std::make_unique<Renderer>(sceneShader_.get());
    sceneFBO_.create(RES_W[2], RES_H[2]);  // default 720p
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
        model = model * glm::scale(glm::mat4(1.0f), glm::vec3(vm.scale));
        model = model * glm::translate(glm::mat4(1.0f), -vm.meshCenter);

        objects.push_back({mesh.mesh, model, mesh.tint, mesh.material});
    }

    return objects;
}

std::vector<PointLight> RenderSystem::collectLights(entt::registry& registry) const {
    std::vector<PointLight> lights;

    auto lightView = registry.view<TransformComponent, LightComponent>();
    for (auto [entity, transform, light] : lightView.each()) {
        lights.push_back({transform.position, light.color, light.radius, light.intensity});
    }

    return lights;
}

void RenderSystem::renderScenePass(const CameraState& camera,
                                   const std::vector<RenderObject>& objects,
                                   const std::vector<RenderObject>& viewmodelObjects,
                                   const std::vector<PointLight>& lights) {
    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    renderer_->drawScene(objects, lights, camera.viewMatrix, camera.projectionMatrix, camera.position);

    if (!viewmodelObjects.empty()) {
        glDepthRange(0.0, 0.01);
        renderer_->drawScene(viewmodelObjects, lights, camera.viewMatrix, camera.projectionMatrix, camera.position);
        glDepthRange(0.0, 1.0);
    }

    sceneFBO_.unbind();
    glDisable(GL_DEPTH_TEST);
}

void RenderSystem::renderDitherPass(Application& app, const CameraState& camera) {
    int displayW = app.window().width();
    int displayH = app.window().height();
    glClear(GL_COLOR_BUFFER_BIT);

    debugParams_.dither.nearPlane = 0.1f;
    debugParams_.dither.farPlane  = 100.0f;

    ditherPass_.apply(sceneFBO_.colorTexture(),
                      sceneFBO_.depthTexture(),
                      sceneFBO_.normalTexture(),
                      debugParams_.dither,
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
                                  std::vector<PointLight>& lights,
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
    CameraState camera = captureCamera(registry);
    std::vector<RenderObject> objects = collectSceneObjects(registry);
    std::vector<RenderObject> viewmodelObjects = collectViewmodelObjects(registry, camera, deltaTime);
    std::vector<PointLight> lights = collectLights(registry);

    renderScenePass(camera, objects, viewmodelObjects, lights);
    renderDitherPass(app, camera);

    GLFWwindow* win = app.window().handle();
    if (glfwGetKey(win, GLFW_KEY_F1) == GLFW_PRESS && !f1Pressed_) {
        f1Pressed_ = true;
        overlaysVisible_ = !overlaysVisible_;
    }
    if (glfwGetKey(win, GLFW_KEY_F1) == GLFW_RELEASE) {
        f1Pressed_ = false;
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
