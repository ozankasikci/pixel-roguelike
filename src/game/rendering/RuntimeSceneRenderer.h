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
    const SceneRenderPipelineStats& pipelineStats() const { return pipeline_.lastStats(); }

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
    void collectSceneObjects(entt::registry& registry,
                             std::vector<RenderObject>& out) const;
    void collectViewmodelObjects(entt::registry& registry,
                                 const CameraState& camera,
                                 float deltaTime,
                                 std::vector<RenderObject>& out) const;
    void collectLights(entt::registry& registry,
                       const DebugParams& params,
                       std::vector<RenderLight>& out) const;
    void collectReflectionProbes(entt::registry& registry,
                                 std::vector<RenderReflectionProbeInput>& out) const;
    void updateDebugParams(DebugParams& params,
                           const CameraState& camera,
                           float deltaTime,
                           std::size_t drawCalls) const;

    SceneRenderPipeline pipeline_;
    ReflectionProbeRenderer reflectionProbeRenderer_;
    MaterialTextureLibrary materialTextureLibrary_;

    // Reused per-frame scratch vectors (mutable to allow const callers)
    mutable std::vector<RenderObject> scene_objects_;
    mutable std::vector<RenderObject> viewmodel_objects_;
    mutable std::vector<RenderLight> lights_;
    mutable std::vector<RenderReflectionProbeInput> reflection_probes_;
    mutable RenderReflectionProbeState active_reflection_probe_;
};
