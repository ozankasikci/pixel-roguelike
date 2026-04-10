#pragma once

#include <cstdint>
#include <string>

#include <glm/vec3.hpp>

#include "engine/audio/VoiceHandle.h"

namespace audio {

class BusGraph; // forward declaration

enum class VoiceState { Playing, Virtual, Stopping, Stopped };

struct Voice {
    engine::audio::VoiceHandle handle;
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    uint8_t priority = 128; // 0=lowest, 255=highest
    std::string busName = "SFX";
    bool is3D = true;
    bool looping = false;
    VoiceState state = VoiceState::Stopped;
    float refDistance = 1.0f;
    float maxDistance = 50.0f;
    float elapsedTime = 0.0f;
    float occlusion = 0.0f; // 0=clear, 1=blocked
    int sourceIndex = -1;   // -1 = virtual
    uint32_t bufferHandle = 0;

    /// Inverse-distance-clamped attenuation (matches AL_INVERSE_DISTANCE_CLAMPED).
    /// Returns 1.0 for 2D voices.
    float computeDistanceAttenuation(const glm::vec3& listenerPos) const;

    /// Composite audibility score used for voice-stealing decisions.
    /// Incorporates volume, distance attenuation, bus effective volume, and priority.
    float computeAudibility(const glm::vec3& listenerPos, const BusGraph& graph) const;
};

} // namespace audio
