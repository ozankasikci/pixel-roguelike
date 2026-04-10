#pragma once

#include <memory>
#include <string>

#include <glm/vec3.hpp>

#include "engine/audio/VoiceHandle.h"
#include "engine/audio/mix/BusGraph.h"
#include "engine/audio/mix/OcclusionProcessor.h"
#include "engine/audio/events/EventRegistry.h"

namespace engine::audio {

struct AudioEngineConfig {
    int voicePoolSize = 32;
    int reverbSlots = 2;
    float occlusionQueryRate = 10.0f;
    std::string audioBasePath = "assets/audio";
    std::string eventDefPath = "assets/audio/sound_events.json";
};

class AudioEngine {
public:
    explicit AudioEngine(const AudioEngineConfig& config = {});
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /// Open AL device, initialize source pool, EFX slots, load event registry,
    /// and preload WAV files referenced by sound events.
    bool init();

    /// Stop all playback, release all AL resources, and shut down subsystems.
    void shutdown();

    /// Play a one-shot 3D sound event at the given position.
    VoiceHandle play(const std::string& eventName, const glm::vec3& position,
                     float volume = 1.0f, float pitch = 1.0f);

    /// Play a one-shot 2D (non-spatial) sound event.
    VoiceHandle play(const std::string& eventName);

    /// Play a looping 3D sound event at the given position.
    VoiceHandle playLooping(const std::string& eventName, const glm::vec3& position,
                            float volume = 1.0f, float pitch = 1.0f);

    /// Stop a voice. If fadeTime > 0, transitions to Stopping state.
    void stop(VoiceHandle handle, float fadeTime = 0.0f);

    /// Update the 3D position of a playing voice.
    void updateVoicePosition(VoiceHandle handle, const glm::vec3& position);

    /// Start streaming music from an OGG file.
    void playMusic(const std::string& path, bool loop = true);

    /// Stop the music stream.
    void stopMusic();

    /// Start streaming ambient audio from an OGG file.
    void playAmbient(const std::string& path, bool loop = true);

    /// Stop the ambient stream.
    void stopAmbient();

    /// Update listener position and orientation for 3D spatialization.
    void setListenerTransform(const glm::vec3& position, const glm::vec3& forward,
                              const glm::vec3& up);

    /// Set the volume of a named bus (Master, SFX, Music, Ambient).
    void setBusVolume(const std::string& busName, float volume);

    /// Mute or unmute a named bus.
    void setBusMute(const std::string& busName, bool mute);

    /// Begin transitioning to a named reverb preset.
    void setReverbPreset(const std::string& presetName, float transitionTime = 0.5f);

    /// Install a raycast callback for occlusion processing.
    void setRaycastFunc(RaycastFunc func);

    /// Stop all active voices immediately (for session reset).
    void stopAll();

    /// Per-frame update: voice scoring/virtualization, occlusion, reverb transitions,
    /// sync voices to AL sources, update streams.
    void update(float deltaTime);

    /// Direct access to the bus graph for volume queries.
    BusGraph& busGraph();

    /// Direct access to the event registry for lookups.
    EventRegistry& eventRegistry();

private:
    /// Push voice state (buffer, position, gain, pitch, looping) to AL sources.
    void syncVoicesToSources();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace engine::audio
