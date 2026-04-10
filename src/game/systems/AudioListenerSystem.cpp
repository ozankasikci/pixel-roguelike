#include "game/systems/AudioListenerSystem.h"

#include "engine/audio/AudioEngine.h"
#include "engine/core/Application.h"
#include "game/components/AudioSourceComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <cmath>

void updateAudioListener(GameRegistry& registry, engine::audio::AudioEngine& audio) {
    // --- Sync listener transform from primary camera ---
    auto camera_view = registry.view<PrimaryCameraTag, TransformComponent>();
    for (auto [entity, transform] : camera_view.each()) {
        // Compute forward and up vectors from euler rotation (matches CameraSystem pattern)
        const float yawRad   = glm::radians(transform.rotation.y);
        const float pitchRad = glm::radians(transform.rotation.x);
        glm::vec3 forward{
            std::cos(pitchRad) * std::sin(yawRad),
           -std::sin(pitchRad),
            std::cos(pitchRad) * std::cos(yawRad)
        };
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        audio.setListenerTransform(transform.position, forward, up);
        break; // Only one primary camera
    }

    // --- Process AudioSourceComponent triggers ---
    auto source_view = registry.view<AudioSourceComponent, TransformComponent>();
    for (auto [entity, source, transform] : source_view.each()) {
        if (!source.triggerPlay) continue;
        source.triggerPlay = false;

        if (source.eventName.empty()) continue;

        if (source.loop) {
            audio.playLooping(source.eventName, transform.position,
                              source.volume, source.pitch);
        } else if (source.is3D) {
            audio.play(source.eventName, transform.position,
                       source.volume, source.pitch);
        } else {
            audio.play(source.eventName);
        }
        source.playing = true;
    }
}

// ---------------------------------------------------------------------------
// AudioListenerSystem
// ---------------------------------------------------------------------------

AudioListenerSystem::AudioListenerSystem(engine::audio::AudioEngine& audio)
    : audio_(audio)
{}

void AudioListenerSystem::init(Application& /*app*/) {}

void AudioListenerSystem::update(Application& app, float /*deltaTime*/) {
    updateAudioListener(app.registry(), audio_);
}

void AudioListenerSystem::shutdown() {}
