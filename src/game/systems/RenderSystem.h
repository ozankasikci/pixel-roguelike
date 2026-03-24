#pragma once
#include "engine/core/System.h"
#include "engine/rendering/Framebuffer.h"
#include "engine/rendering/DitherPass.h"
#include "engine/rendering/Renderer.h"
#include "engine/rendering/Shader.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/ui/Screenshot.h"
#include <memory>
#include <string>

class RenderSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

    // Access for Plan 04 wiring
    DebugParams& debugParams() { return debugParams_; }
    Framebuffer& sceneFBO() { return sceneFBO_; }

    void enableAutoScreenshot(const std::string& path, int delayFrames = 10);

private:
    Framebuffer sceneFBO_;
    std::unique_ptr<Shader> sceneShader_;
    std::unique_ptr<Renderer> renderer_;
    DitherPass ditherPass_;
    ImGuiLayer imguiLayer_;
    DebugParams debugParams_;
    AutoScreenshot autoCapture_;
    bool overlaysVisible_ = false;
    bool f1Pressed_ = false;
    bool f12Pressed_ = false;

    // Internal resolution presets (same as current main.cpp)
    static constexpr int RES_W[] = {854, 960, 1280};
    static constexpr int RES_H[] = {480, 540,  720};
};
