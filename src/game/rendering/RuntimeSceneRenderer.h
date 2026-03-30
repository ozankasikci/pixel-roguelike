#pragma once

#include "engine/rendering/SceneRenderPipeline.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <vector>

class ContentRegistry;

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

    Framebuffer& sceneFBO() { return pipeline_.sceneFBO(); }
    const Framebuffer& sceneFBO() const { return pipeline_.sceneFBO(); }

private:
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
    void updateDebugParams(DebugParams& params,
                           const CameraState& camera,
                           float deltaTime,
                           std::size_t drawCalls) const;

    SceneRenderPipeline pipeline_;
    MaterialTextureLibrary materialTextureLibrary_;
};
