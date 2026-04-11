#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace particles {

enum class BlendMode : uint8_t { Additive, AlphaBlend };
enum class SimulationSpace : uint8_t { World, Local };
enum class EmitterState : uint8_t { Playing, Stopping, Stopped };

struct alignas(4) ParticleInstance {
    glm::vec3 position;
    uint32_t colorPacked;
    float size;
    float rotation;
    float normalizedAge;
};
static_assert(sizeof(ParticleInstance) == 28, "ParticleInstance must be 28 bytes");

struct ParticleRenderBatch {
    const ParticleInstance* instances = nullptr;
    int count = 0;
    BlendMode blendMode = BlendMode::Additive;
    GLuint texture = 0;
    float emissiveStrength = 1.0f;
    float softParticleFade = 0.5f;
};

inline uint32_t packColorRGBA8(const glm::vec4& c) {
    auto toByte = [](float f) -> uint8_t {
        return static_cast<uint8_t>(glm::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (uint32_t(toByte(c.r)))
         | (uint32_t(toByte(c.g)) << 8)
         | (uint32_t(toByte(c.b)) << 16)
         | (uint32_t(toByte(c.a)) << 24);
}

} // namespace particles
