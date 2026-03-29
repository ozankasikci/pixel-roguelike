#pragma once
#include "engine/core/System.h"
#include "engine/input/InputSystem.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/ui/Screenshot.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/RuntimeSceneRenderer.h"

#include <string>

struct InteractionPromptState;

class RenderSystem : public System {
public:
    explicit RenderSystem(InputSystem& input) : input_(input) {}

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

    // Access for Plan 04 wiring
    DebugParams& debugParams() { return debugParams_; }
    Framebuffer& sceneFBO() { return runtimeRenderer_.sceneFBO(); }

    void enableAutoScreenshot(const std::string& path, int delayFrames = 10);

private:
    InteractionPromptState& ensurePromptState(entt::registry& registry) const;
    void renderOverlays(Application& app,
                        entt::registry& registry,
                        std::vector<RenderLight>& lights,
                        InteractionPromptState& prompt);
    void handleResolutionChange();
    void handleCapture(Application& app, int displayW, int displayH);

    ImGuiLayer imguiLayer_;
    DebugParams debugParams_;
    AutoScreenshot autoCapture_;
    InputSystem& input_;
    bool overlaysVisible_ = false;
    bool f1Pressed_ = false;
    bool f12Pressed_ = false;
    RuntimeSceneRenderer runtimeRenderer_;
    RuntimeEnvironmentSyncState environmentSyncState_;

    // Internal resolution presets (same as current main.cpp)
    static constexpr int RES_W[] = {854, 960, 1280};
    static constexpr int RES_H[] = {480, 540,  720};
};
