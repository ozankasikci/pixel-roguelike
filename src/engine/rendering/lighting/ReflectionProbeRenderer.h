#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

struct SceneRenderInput;
class SceneRenderPipeline;

struct RenderReflectionProbeInput {
    std::uint64_t id = 0;
    glm::vec3 center{0.0f};
    glm::vec3 extents{4.0f, 3.0f, 4.0f};
    float blendDistance = 1.0f;
    float intensity = 1.0f;
    bool boxProjection = true;
    bool dirty = true;
};

struct RenderReflectionProbeState {
    GLuint cubemap = 0;
    glm::vec3 center{0.0f};
    glm::vec3 extents{4.0f, 3.0f, 4.0f};
    float blendDistance = 1.0f;
    float intensity = 1.0f;
    int boxProjection = 1;
    int mipCount = 1;
    bool enabled = false;
};

class ReflectionProbeRenderer {
public:
    void init();
    void shutdown();

    std::vector<std::uint64_t> updateDirtyProbes(const std::vector<RenderReflectionProbeInput>& probes,
                                                 const SceneRenderInput& sceneInput,
                                                 SceneRenderPipeline& pipeline);

    RenderReflectionProbeState selectNearestProbe(const glm::vec3& worldPosition,
                                                  const std::vector<RenderReflectionProbeInput>& probes) const;

private:
    struct ProbeSlot {
        std::uint64_t id = 0;
        GLuint cubemap = 0;
        bool occupied = false;
        RenderReflectionProbeState state{};
    };

    void ensureCaptureResources();
    void ensureProbeCubemap(ProbeSlot& slot);
    ProbeSlot* findOrAllocateSlot(std::uint64_t id);
    void captureProbe(ProbeSlot& slot,
                      const RenderReflectionProbeInput& probe,
                      const SceneRenderInput& sceneInput,
                      SceneRenderPipeline& pipeline);

    static constexpr int kProbeResolution = 128;
    static constexpr int kMaxLocalProbes = 4;

    GLuint captureFbo_ = 0;
    GLuint captureColorTex_ = 0;
    std::vector<ProbeSlot> slots_;
};
