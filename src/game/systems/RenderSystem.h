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
#include <vector>

struct InteractionPromptState;

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
    struct CameraState {
        glm::vec3 position{0.0f};
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };

    CameraState captureCamera(entt::registry& registry) const;
    std::vector<RenderObject> collectSceneObjects(entt::registry& registry) const;
    std::vector<RenderObject> collectViewmodelObjects(entt::registry& registry,
                                                      const CameraState& camera,
                                                      float deltaTime) const;
    std::vector<PointLight> collectLights(entt::registry& registry) const;
    void renderScenePass(const CameraState& camera,
                         const std::vector<RenderObject>& objects,
                         const std::vector<RenderObject>& viewmodelObjects,
                         const std::vector<PointLight>& lights);
    void renderDitherPass(Application& app, const CameraState& camera);
    InteractionPromptState& ensurePromptState(entt::registry& registry) const;
    void updateDebugParams(const CameraState& camera, float deltaTime, std::size_t drawCalls);
    void renderOverlays(Application& app,
                        entt::registry& registry,
                        std::vector<PointLight>& lights,
                        InteractionPromptState& prompt);
    void handleResolutionChange();
    void handleCapture(Application& app, int displayW, int displayH);

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
