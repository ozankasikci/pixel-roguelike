#include "game/systems/RenderSystem.h"

#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/ViewmodelComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

// constexpr definitions (needed in pre-C++17 out-of-line)
constexpr int RenderSystem::RES_W[];
constexpr int RenderSystem::RES_H[];

void RenderSystem::init(Application& app) {
    runtimeRenderer_.init(app.getService<ContentRegistry>());
    imguiLayer_.init(app.window().handle());
    applyEnvironmentProfile(debugParams_, EnvironmentProfile::Neutral, true);
}

InteractionPromptState& RenderSystem::ensurePromptState(entt::registry& registry) const {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    return ctx.get<InteractionPromptState>();
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

    const int idx = debugParams_.internalResIndex;
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

    const int internalIndex = std::clamp(debugParams_.internalResIndex, 0, 2);
    RuntimeSceneRenderOutput frameOutput;
    runtimeRenderer_.render(registry,
                            debugParams_,
                            deltaTime,
                            RES_W[internalIndex],
                            RES_H[internalIndex],
                            app.window().width(),
                            app.window().height(),
                            0,
                            &environmentSyncState_,
                            true,
                            &frameOutput);

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

    auto& prompt = ensurePromptState(registry);
    renderOverlays(app, registry, frameOutput.lights, prompt);
    handleResolutionChange();
    handleCapture(app, app.window().width(), app.window().height());
}

void RenderSystem::shutdown() {
    runtimeRenderer_.shutdown();
    imguiLayer_.shutdown();
}

void RenderSystem::enableAutoScreenshot(const std::string& path, int delayFrames) {
    overlaysVisible_ = false;
    autoCapture_.enable(path, delayFrames);
    spdlog::info("Auto-screenshot enabled: {}", path);
}
