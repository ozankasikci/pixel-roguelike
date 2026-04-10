#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "engine/audio/VoiceHandle.h"
#include "engine/audio/mix/Voice.h"

namespace engine::audio {

class BusGraph;

class VoiceManager {
public:
    explicit VoiceManager(int poolSize = 32);

    // Non-copyable
    VoiceManager(const VoiceManager&) = delete;
    VoiceManager& operator=(const VoiceManager&) = delete;

    /// Spawn a new voice and return its handle.
    VoiceHandle spawn(const std::string& eventName, const glm::vec3& position,
                                     float volume, float pitch, uint8_t priority,
                                     const std::string& busName, bool is3D, bool looping,
                                     float refDistance, float maxDistance, uint32_t bufferHandle);

    /// Stop a voice. If fadeTime <= 0, sets Stopped immediately; otherwise sets Stopping.
    void stop(VoiceHandle handle, float fadeTime = 0.0f);

    /// Stop all voices matching an event name.
    void stopAllByEvent(const std::string& eventName);

    /// Stop all voices immediately.
    void stopAll();

    /// Update the 3D position of a voice.
    void updatePosition(VoiceHandle handle, const glm::vec3& position);

    /// Per-frame update: remove stopped voices, score audibility, assign source indices.
    void update(const glm::vec3& listenerPos, const BusGraph& graph, float deltaTime);

    /// Find a voice by handle. Returns nullptr if not found or generation mismatch.
    Voice* findVoice(VoiceHandle handle);
    const Voice* findVoice(VoiceHandle handle) const;

    /// Number of voices in Playing or Stopping state (not Virtual or Stopped).
    int activeVoiceCount() const;

    /// Count voices with a given event name (any non-Stopped state).
    int countByEvent(const std::string& eventName) const;

    /// Direct access to the voice list for backend iteration.
    std::vector<Voice>& voices();
    const std::vector<Voice>& voices() const;

private:
    int poolSize_;
    uint32_t nextId_ = 1;
    uint32_t generation_ = 0;
    std::vector<Voice> voices_;
};

} // namespace engine::audio
