#include "game/systems/RenderSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include "game/components/TransformComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/PlayerMovementComponent.h"

#include <glad/gl.h>
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

void RenderSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();

    // --- Find camera entity (expect exactly one) ---
    glm::vec3 cameraPos{0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::vec3 cameraDir{0.0f, 0.0f, -1.0f};

    {
        auto camView = registry.view<TransformComponent, CameraComponent>();
        for (auto [entity, transform, cam] : camView.each()) {
            cameraPos        = transform.position;
            viewMatrix       = cam.viewMatrix;
            projectionMatrix = cam.projectionMatrix;
            cameraDir        = cam.forward;
            break;
        }
    }

    // --- Build render object list from ECS ---
    std::vector<RenderObject> objects;
    {
        auto meshView = registry.view<TransformComponent, MeshComponent>();
        for (auto [entity, transform, mesh] : meshView.each()) {
            if (mesh.mesh == nullptr) continue;
            glm::mat4 model = mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix();
            objects.push_back({mesh.mesh, model});
        }
    }

    // --- Build point light list from ECS ---
    std::vector<PointLight> lights;
    {
        auto lightView = registry.view<TransformComponent, LightComponent>();
        for (auto [entity, transform, light] : lightView.each()) {
            lights.push_back({transform.position, light.color, light.radius, light.intensity});
        }
    }

    // ---- Scene pass: render to FBO (color + normals + depth) ---- mirrors main.cpp lines 141-151
    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    renderer_->drawScene(objects, lights, viewMatrix, projectionMatrix, cameraPos);

    sceneFBO_.unbind();
    glDisable(GL_DEPTH_TEST);

    // ---- Dither pass: edges + fog + Bayer dithering ---- mirrors main.cpp lines 153-166
    int displayW = app.window().width();
    int displayH = app.window().height();
    glClear(GL_COLOR_BUFFER_BIT);

    // Sync near/far with projection
    debugParams_.dither.nearPlane = 0.1f;
    debugParams_.dither.farPlane  = 100.0f;

    ditherPass_.apply(sceneFBO_.colorTexture(),
                      sceneFBO_.depthTexture(),
                      sceneFBO_.normalTexture(),
                      debugParams_.dither,
                      displayW, displayH);

    // ---- ImGui overlay ---- mirrors main.cpp lines 168-179
    imguiLayer_.beginFrame();

    debugParams_.cameraPos   = cameraPos;
    debugParams_.cameraDir   = cameraDir;
    debugParams_.fps         = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
    debugParams_.frameTimeMs = deltaTime * 1000.0f;
    debugParams_.drawCalls   = static_cast<int>(objects.size());

    ImGuiLayer::renderOverlay(debugParams_, lights);

    // Movement debug panel
    {
        auto movView = registry.view<PlayerMovementComponent>();
        for (auto [entity, movement] : movView.each()) {
            ImGuiLayer::renderMovementOverlay(movement, movement.grounded);
            break;
        }
    }

    imguiLayer_.endFrame();

    // Handle resolution change -- mirrors main.cpp lines 182-187
    if (debugParams_.resolutionChanged) {
        int idx = debugParams_.internalResIndex;
        sceneFBO_.resize(RES_W[idx], RES_H[idx]);
        spdlog::info("Internal resolution changed to {}x{}", RES_W[idx], RES_H[idx]);
        debugParams_.resolutionChanged = false;
    }

    // F12: manual screenshot -- mirrors main.cpp lines 189-195
    GLFWwindow* win = app.window().handle();
    if (glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS && !f12Pressed_) {
        f12Pressed_ = true;
        saveScreenshot(displayW, displayH);
    }
    if (glfwGetKey(win, GLFW_KEY_F12) == GLFW_RELEASE) {
        f12Pressed_ = false;
    }

    // Auto-screenshot: capture and exit -- mirrors main.cpp lines 198-202
    if (autoCapture_.tick(displayW, displayH)) {
        spdlog::info("Auto-screenshot captured, exiting");
        // Signal the application to stop (handled in plan 04 when wired into Application)
    }
}

void RenderSystem::shutdown() {
    imguiLayer_.shutdown();
}

void RenderSystem::enableAutoScreenshot(const std::string& path, int delayFrames) {
    autoCapture_.enable(path, delayFrames);
    spdlog::info("Auto-screenshot enabled: {}", path);
}
