#pragma once

#include "engine/rendering/SceneRenderPipeline.h"

#include <glm/glm.hpp>
#include <vector>

struct EnvironmentDefinition;

struct EditorViewportRenderParams {
    // Camera
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::vec3 cameraPosition{0.0f};
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    // Objects and lights (already collected and material-resolved by caller)
    const std::vector<RenderObject>* objects = nullptr;
    const std::vector<RenderLight>* lights = nullptr;
    // Environment
    const EnvironmentDefinition* environment = nullptr;
    // Toggles
    bool shadowsEnabled = true;
    int shadowResolutionIndex = 1;  // 0=512, 1=1024, 2=2048
};

// EditorViewportRenderer wraps SceneRenderPipeline for the editor's edit-mode
// viewport. It provides the same full Phase 4 rendering pipeline (bloom, SSAO,
// CSM directional shadows, LTC area lights, emissive materials) as the runtime.
//
// The caller is responsible for collecting and material-resolving RenderObjects
// and RenderLights before calling render(). This keeps material resolution in
// main.cpp where the MaterialTextureLibrary lives.
class EditorViewportRenderer {
public:
    EditorViewportRenderer() = default;
    ~EditorViewportRenderer() = default;

    EditorViewportRenderer(const EditorViewportRenderer&) = delete;
    EditorViewportRenderer& operator=(const EditorViewportRenderer&) = delete;

    void init();
    void shutdown();

    // Render the edit-mode viewport into targetFBO using the full pipeline.
    // outputW/outputH are the pixel dimensions of the render target.
    void render(const EditorViewportRenderParams& params,
                int outputW, int outputH,
                GLuint targetFBO);

    // Access to underlying pipeline's LTC data (for asset preview renderer D-12).
    const LtcData& ltcData() const { return pipeline_.ltcData(); }

private:
    SceneRenderPipeline pipeline_;
};
