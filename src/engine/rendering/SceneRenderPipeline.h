#pragma once

#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/lighting/CascadedShadowMap.h"
#include "engine/rendering/lighting/LtcData.h"
#include "engine/rendering/lighting/ReflectionProbeRenderer.h"
#include "engine/rendering/lighting/RenderLight.h"
#include "engine/rendering/lighting/ShadowMap.h"
#include "engine/rendering/post/BloomPass.h"
#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/post/PostProcessParams.h"
#include "engine/rendering/post/SkyTextureLibrary.h"
#include "engine/rendering/post/SsaoPass.h"
#include "engine/rendering/post/StylizePass.h"

#include <array>
#include <memory>
#include <vector>

class Shader;

// Per-frame timing and draw statistics reported by SceneRenderPipeline.
// All values are updated after each render() call.
struct SceneRenderPipelineStats {
    double totalRenderMs = 0.0;
    double shadowPassMs = 0.0;
    double scenePassMs = 0.0;
    double bloomMs = 0.0;
    double ssaoMs = 0.0;
    double compositeMs = 0.0;
    int drawCalls = 0;
    int objectCount = 0;
    int lightCount = 0;
    int culledCount = 0;       // Objects culled by frustum test
    int shadowCulledCount = 0; // Objects culled across all shadow passes
};

// Input struct for SceneRenderPipeline::render(). All fields are engine-layer types.
// No game-layer types allowed here.
struct SceneRenderInput {
    const std::vector<RenderObject>* objects = nullptr;
    const std::vector<RenderObject>* viewmodelObjects = nullptr;
    const std::vector<RenderLight>* lights = nullptr;
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::vec3 cameraPosition{0.0f};
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    const PostProcessParams* postParams = nullptr;
    const RenderReflectionProbeState* reflectionProbe = nullptr;
    LightingEnvironment lightingEnvironment;
    bool shadowsEnabled = true;
    int shadowResolutionIndex = 1;  // 0=512, 1=1024, 2=2048
};

// SceneRenderPipeline orchestrates the full rendering pipeline:
//   shadow pass -> CSM -> scene pass -> bloom -> SSAO -> composite -> stylize
//
// This class lives in engine_rendering and has NO game-layer dependencies.
// RuntimeSceneRenderer (game layer) composes this pipeline as a member.
class SceneRenderPipeline {
public:
    SceneRenderPipeline() = default;
    ~SceneRenderPipeline() = default;

    SceneRenderPipeline(const SceneRenderPipeline&) = delete;
    SceneRenderPipeline& operator=(const SceneRenderPipeline&) = delete;

    void init();
    void shutdown();

    // Execute the full pipeline: shadow pass, CSM, scene pass, bloom, SSAO, composite, stylize.
    // internalWidth/Height is the render resolution; outputWidth/Height is the display resolution.
    // targetFramebuffer = 0 renders to the default framebuffer (screen).
    void render(const SceneRenderInput& input,
                int internalWidth,
                int internalHeight,
                int outputWidth,
                int outputHeight,
                GLuint targetFramebuffer = 0);

    // Accessor for sceneFBO needed by consumers that must read rendered pixels
    // (e.g., screenshot, editor overlay, or external texture sampling).
    Framebuffer& sceneFBO() { return sceneFBO_; }
    const Framebuffer& sceneFBO() const { return sceneFBO_; }

    // LTC data accessor for consumers that need to bind the LTC lookup textures
    // to additional shader programs (e.g., asset preview renderer per D-12).
    const LtcData& ltcData() const { return ltcData_; }

    // Per-frame timing stats updated after each render() call.
    const SceneRenderPipelineStats& lastStats() const { return lastStats_; }

private:
    void ensureFramebuffers(int w, int h, const PostProcessParams* postParams = nullptr);
    void assignShadowSlots(std::vector<RenderLight>& lights, bool enabled);
    glm::mat4 buildShadowMatrix(const RenderLight& light) const;
    void renderShadowPass(const std::vector<RenderObject>& objects,
                          std::vector<RenderLight>& lights,
                          const SceneRenderInput& input,
                          ShadowRenderData& shadowData);
    void renderScenePass(const SceneRenderInput& input,
                         const std::vector<RenderLight>& lights,
                         const ShadowRenderData& shadowData,
                         int internalWidth,
                         int internalHeight);
    void renderPostProcess(const SceneRenderInput& input,
                           int outputWidth,
                           int outputHeight,
                           GLuint targetFramebuffer);

    SceneRenderPipelineStats lastStats_;
    Framebuffer sceneFBO_;
    Framebuffer compositeFBO_;
    std::unique_ptr<Shader> sceneShader_;
    std::unique_ptr<Shader> shadowShader_;
    std::unique_ptr<Renderer> renderer_;
    BloomPass bloomPass_;
    SsaoPass ssaoPass_;
    CompositePass compositePass_;
    StylizePass stylizePass_;
    SkyTextureLibrary skyTextures_;
    LtcData ltcData_;
    std::array<ShadowMap, kMaxShadowedSpotLights> shadowMaps_{};
    CascadedShadowMap csmShadowMap_;

    // Standalone depth texture that receives a copy of the scene depth buffer
    // before the shading pass, used for screen-space contact shadows.
    GLuint depthPrePassTex_ = 0;
    int depthPrePassW_ = 0;
    int depthPrePassH_ = 0;
};
