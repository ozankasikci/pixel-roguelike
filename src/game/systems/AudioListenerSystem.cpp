#include "game/systems/AudioListenerSystem.h"

#include "engine/audio/AudioSystem.h"
#include "engine/core/Application.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <cmath>

AudioSystem& audioSystem(Application& app) {
    return *app.getService<AudioSystem*>();
}

void updateAudioListener(entt::registry& registry, AudioSystem& audio) {
    auto view = registry.view<PrimaryCameraTag, TransformComponent>();
    for (auto [entity, transform] : view.each()) {
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
}

// ---------------------------------------------------------------------------
// AudioListenerSystem
// ---------------------------------------------------------------------------

AudioListenerSystem::AudioListenerSystem(AudioSystem& audio)
    : audio_(audio)
{}

void AudioListenerSystem::init(Application& /*app*/) {}

void AudioListenerSystem::update(Application& app, float /*deltaTime*/) {
    updateAudioListener(app.registry(), audio_);
}

void AudioListenerSystem::shutdown() {}
