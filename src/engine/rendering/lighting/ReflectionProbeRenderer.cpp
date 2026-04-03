#include "engine/rendering/lighting/ReflectionProbeRenderer.h"

#include "engine/rendering/SceneRenderPipeline.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace {

constexpr std::array<glm::vec3, 6> kCaptureDirections{
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(-1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 0.0f, -1.0f),
};

constexpr std::array<glm::vec3, 6> kCaptureUps{
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 0.0f, -1.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
};

int mipCountForResolution(int resolution) {
    int levels = 1;
    while (resolution > 1) {
        resolution >>= 1;
        ++levels;
    }
    return levels;
}

} // namespace

void ReflectionProbeRenderer::init() {
    ensureCaptureResources();
    if (slots_.empty()) {
        slots_.resize(kMaxLocalProbes);
    }
}

void ReflectionProbeRenderer::shutdown() {
    for (auto& slot : slots_) {
        if (slot.cubemap != 0) {
            glDeleteTextures(1, &slot.cubemap);
            slot.cubemap = 0;
        }
        slot = ProbeSlot{};
    }
    slots_.clear();

    if (captureColorTex_ != 0) {
        glDeleteTextures(1, &captureColorTex_);
        captureColorTex_ = 0;
    }
    if (captureFbo_ != 0) {
        glDeleteFramebuffers(1, &captureFbo_);
        captureFbo_ = 0;
    }
}

void ReflectionProbeRenderer::ensureCaptureResources() {
    if (captureFbo_ != 0 && captureColorTex_ != 0) {
        return;
    }

    glGenTextures(1, &captureColorTex_);
    glBindTexture(GL_TEXTURE_2D, captureColorTex_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA16F,
                 kProbeResolution,
                 kProbeResolution,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &captureFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, captureColorTex_, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ReflectionProbeRenderer::ensureProbeCubemap(ProbeSlot& slot) {
    if (slot.cubemap != 0) {
        return;
    }

    glGenTextures(1, &slot.cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, slot.cubemap);
    const int mipCount = mipCountForResolution(kProbeResolution);
    for (int face = 0; face < 6; ++face) {
        for (int mip = 0; mip < mipCount; ++mip) {
            const int size = std::max(1, kProbeResolution >> mip);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                         mip,
                         GL_RGBA16F,
                         size,
                         size,
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         nullptr);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

ReflectionProbeRenderer::ProbeSlot* ReflectionProbeRenderer::findOrAllocateSlot(std::uint64_t id) {
    for (auto& slot : slots_) {
        if (slot.occupied && slot.id == id) {
            return &slot;
        }
    }
    for (auto& slot : slots_) {
        if (!slot.occupied) {
            slot.occupied = true;
            slot.id = id;
            return &slot;
        }
    }
    return nullptr;
}

void ReflectionProbeRenderer::captureProbe(ProbeSlot& slot,
                                           const RenderReflectionProbeInput& probe,
                                           const SceneRenderInput& sceneInput,
                                           SceneRenderPipeline& pipeline) {
    ensureCaptureResources();
    ensureProbeCubemap(slot);

    const float farPlane = std::max(std::max(probe.extents.x, std::max(probe.extents.y, probe.extents.z)) * 3.0f, 8.0f);

    for (int face = 0; face < 6; ++face) {
        SceneRenderInput captureInput = sceneInput;
        captureInput.viewmodelObjects = nullptr;
        captureInput.reflectionProbe = nullptr;
        captureInput.viewMatrix = glm::lookAt(probe.center,
                                              probe.center + kCaptureDirections[static_cast<std::size_t>(face)],
                                              kCaptureUps[static_cast<std::size_t>(face)]);
        captureInput.projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);
        captureInput.cameraPosition = probe.center;
        captureInput.nearPlane = 0.1f;
        captureInput.farPlane = farPlane;

        pipeline.render(captureInput,
                        kProbeResolution,
                        kProbeResolution,
                        kProbeResolution,
                        kProbeResolution,
                        captureFbo_);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, captureFbo_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, slot.cubemap);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                            0,
                            0,
                            0,
                            0,
                            0,
                            kProbeResolution,
                            kProbeResolution);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, slot.cubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    slot.state.enabled = true;
    slot.state.cubemap = slot.cubemap;
    slot.state.center = probe.center;
    slot.state.extents = probe.extents;
    slot.state.blendDistance = probe.blendDistance;
    slot.state.intensity = probe.intensity;
    slot.state.boxProjection = probe.boxProjection ? 1 : 0;
    slot.state.mipCount = mipCountForResolution(kProbeResolution);
}

std::vector<std::uint64_t> ReflectionProbeRenderer::updateDirtyProbes(const std::vector<RenderReflectionProbeInput>& probes,
                                                                      const SceneRenderInput& sceneInput,
                                                                      SceneRenderPipeline& pipeline) {
    std::vector<std::uint64_t> capturedIds;
    ensureCaptureResources();
    if (slots_.empty()) {
        slots_.resize(kMaxLocalProbes);
    }

    for (const auto& probe : probes) {
        ProbeSlot* slot = findOrAllocateSlot(probe.id);
        if (slot == nullptr) {
            continue;
        }
        slot->state.center = probe.center;
        slot->state.extents = probe.extents;
        slot->state.blendDistance = probe.blendDistance;
        slot->state.intensity = probe.intensity;
        slot->state.boxProjection = probe.boxProjection ? 1 : 0;
        if (probe.dirty || slot->cubemap == 0 || !slot->state.enabled) {
            captureProbe(*slot, probe, sceneInput, pipeline);
            capturedIds.push_back(probe.id);
        }
    }

    return capturedIds;
}

RenderReflectionProbeState ReflectionProbeRenderer::selectNearestProbe(const glm::vec3& worldPosition,
                                                                       const std::vector<RenderReflectionProbeInput>& probes) const {
    float bestDistance = std::numeric_limits<float>::max();
    const ProbeSlot* bestSlot = nullptr;

    for (const auto& probe : probes) {
        const ProbeSlot* slot = nullptr;
        for (const auto& candidate : slots_) {
            if (candidate.occupied && candidate.id == probe.id && candidate.state.enabled && candidate.cubemap != 0) {
                slot = &candidate;
                break;
            }
        }
        if (slot == nullptr) {
            continue;
        }

        const float distance = glm::distance(worldPosition, probe.center);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestSlot = slot;
        }
    }

    if (bestSlot == nullptr) {
        return RenderReflectionProbeState{};
    }
    return bestSlot->state;
}
