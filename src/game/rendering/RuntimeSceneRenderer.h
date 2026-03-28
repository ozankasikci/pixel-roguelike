#pragma once

#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/lighting/ShadowMap.h"
#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/post/StylizePass.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <array>
#include <memory>
#include <vector>

class ContentRegistry;
class Shader;

struct RuntimeSceneRenderOutput {
    std::vector<RenderLight> lights;
    std::size_t drawCalls = 0;
};

class RuntimeSceneRenderer {
public:
    void init(const ContentRegistry& content);
    void shutdown();
    void reloadContent(const ContentRegistry& content);
    std::size_t prewarmMaterialResources(entt::registry& registry);

    void render(entt::registry& registry,
                DebugParams& params,
                float deltaTime,
                int internalWidth,
                int internalHeight,
                int outputWidth,
                int outputHeight,
                GLuint targetFramebuffer = 0,
                RuntimeEnvironmentSyncState* environmentState = nullptr,
                bool preserveEnvironmentOverrides = false,
                RuntimeSceneRenderOutput* output = nullptr);

    Framebuffer& sceneFBO() { return sceneFBO_; }
    const Framebuffer& sceneFBO() const { return sceneFBO_; }

private:
    static constexpr int kMaxShadowedSpotLights = 2;

    struct CameraState {
        glm::vec3 position{0.0f};
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float moveSpeed = 3.0f;
    };

    CameraState captureCamera(entt::registry& registry, float aspect) const;
    std::vector<RenderObject> collectSceneObjects(entt::registry& registry) const;
    std::vector<RenderObject> collectViewmodelObjects(entt::registry& registry,
                                                      const CameraState& camera,
                                                      float deltaTime) const;
    std::vector<RenderLight> collectLights(entt::registry& registry,
                                           const DebugParams& params) const;
    void assignShadowSlots(std::vector<RenderLight>& lights, const DebugParams& params) const;
    LightingEnvironment lightingEnvironment(const DebugParams& params) const;
    int shadowResolution(const DebugParams& params) const;
    glm::mat4 buildShadowMatrix(const RenderLight& light) const;
    void renderShadowPass(const std::vector<RenderObject>& objects,
                          const std::vector<RenderLight>& lights,
                          const DebugParams& params,
                          ShadowRenderData& shadowData);
    void renderScenePass(const CameraState& camera,
                         const std::vector<RenderObject>& objects,
                         const std::vector<RenderObject>& viewmodelObjects,
                         const std::vector<RenderLight>& lights,
                         const DebugParams& params,
                         int internalWidth,
                         int internalHeight);
    void renderPostProcess(const CameraState& camera,
                           DebugParams& params,
                           int outputWidth,
                           int outputHeight,
                           GLuint targetFramebuffer);
    void updateDebugParams(DebugParams& params,
                           const CameraState& camera,
                           float deltaTime,
                           std::size_t drawCalls) const;
    void ensureFramebuffers(int internalWidth, int internalHeight);

    Framebuffer sceneFBO_;
    Framebuffer compositeFBO_;
    std::unique_ptr<Shader> sceneShader_;
    std::unique_ptr<Shader> shadowShader_;
    std::unique_ptr<Renderer> renderer_;
    CompositePass compositePass_;
    StylizePass stylizePass_;
    MaterialTextureLibrary materialTextureLibrary_;
    std::array<ShadowMap, kMaxShadowedSpotLights> shadowMaps_{};
};
