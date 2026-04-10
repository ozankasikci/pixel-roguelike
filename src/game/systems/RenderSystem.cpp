#include "game/systems/RenderSystem.h"

#include "engine/camera/CameraManager.h"
#include "engine/camera/CameraState.h"
#include "engine/core/Application.h"
#include "game/runtime/RuntimeGameSession.h"
#include "engine/core/Window.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/ViewmodelComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/modules/interaction/InteractionPromptState.h"
#include "game/modules/interaction/InteractionSystem.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/GameOverlays.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <csignal>
#endif

namespace {
volatile int g_screenshotRequested = 0;

#ifndef _WIN32
void signalScreenshotHandler(int) {
    g_screenshotRequested = 1;
}

bool installSignalHandler() {
    std::signal(SIGUSR1, signalScreenshotHandler);
    return true;
}

[[maybe_unused]] static bool s_signalInstalled = installSignalHandler();
#endif
} // namespace

void RenderSystem::init(Application& app) {
    runtimeRenderer_.init(app.getService<ContentRegistry>());
    imguiLayer_.init(app.window().handle());
    applyEnvironmentProfile(debugParams_, EnvironmentProfile::Default, true);
}

void RenderSystem::renderOverlays(Application& app,
                                  GameRegistry& registry,
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
        GameOverlays::renderInventory(menu, session, content, equipment);
    }

    if (overlaysVisible_) {
        ImGuiLayer::renderOverlay(debugParams_, lights);

        auto movView = registry.view<PlayerMovementComponent>();
        for (auto [entity, movement] : movView.each()) {
            GameOverlays::renderMovementOverlay(movement, movement.grounded);
            break;
        }

        auto vmView = registry.view<MeshComponent, ViewmodelComponent>();
        for (auto [entity, mesh, vm] : vmView.each()) {
            GameOverlays::renderViewmodelOverlay(vm);
            break;
        }
    }

    if (prompt.visible && !inventoryOpen) {
        GameOverlays::renderInteractionPrompt(prompt.text.c_str(), prompt.busy);
    }

    imguiLayer_.endFrame();
}

void RenderSystem::handleResolutionChange() {
    if (!debugParams_.resolutionChanged) {
        return;
    }

    const int idx = std::clamp(debugParams_.internalResIndex,
                               0,
                               static_cast<int>(kRenderResolutionPresets.size()) - 1);
    const RenderResolutionPreset& preset = kRenderResolutionPresets[static_cast<std::size_t>(idx)];
    spdlog::info("Internal resolution changed to {}x{}", preset.width, preset.height);
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

    if (g_screenshotRequested) {
        g_screenshotRequested = 0;
        saveScreenshot(displayW, displayH, "/tmp/game_screenshot.png");
    }

    if (autoCapture_.tick(displayW, displayH)) {
        spdlog::info("Auto-screenshot captured, exiting");
        app.requestQuit();
    }
}

void RenderSystem::update(Application& app, float deltaTime) {
    auto* sessionPtr = app.tryGetService<RuntimeGameSession*>();
    if (!sessionPtr || !*sessionPtr) return;
    auto& registry = (*sessionPtr)->registry();
    const bool escapeOpenedCursor = input_.isKeyJustPressed(GLFW_KEY_ESCAPE) && !input_.isCursorLocked();

    const int internalIndex = std::clamp(debugParams_.internalResIndex,
                                         0,
                                         static_cast<int>(kRenderResolutionPresets.size()) - 1);
    const RenderResolutionPreset& preset = kRenderResolutionPresets[static_cast<std::size_t>(internalIndex)];
    RuntimeSceneRenderOutput frameOutput;
    const CameraState fallbackCamera;
    const CameraState& cameraState = registry.ctx().contains<CameraManager*>()
        ? registry.ctx().get<CameraManager*>()->getState()
        : fallbackCamera;
    runtimeRenderer_.render(registry,
                            cameraState,
                            debugParams_,
                            deltaTime,
                            preset.width,
                            preset.height,
                            app.window().width(),
                            app.window().height(),
                            0,
                            &environmentSyncState_,
                            true,
                            &frameOutput);

    // Log render pipeline stats periodically
    {
        static int renderLogCounter = 0;
        if (++renderLogCounter >= 300) {
            const auto& stats = runtimeRenderer_.pipelineStats();
            spdlog::info("[Render] total {:.1f}ms | shadow {:.1f}ms | scene {:.1f}ms | bloom {:.1f}ms | ssao {:.1f}ms | composite {:.1f}ms | draws {} | lights {} | culled {}",
                         stats.totalRenderMs, stats.shadowPassMs, stats.scenePassMs,
                         stats.bloomMs, stats.ssaoMs, stats.compositeMs,
                         stats.drawCalls, stats.lightCount, stats.culledCount);
            renderLogCounter = 0;
        }
    }

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

    auto& prompt = ensureInteractionPromptState(registry);
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
