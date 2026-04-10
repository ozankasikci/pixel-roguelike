#include "game/systems/AudioListenerSystem.h"

#include "engine/audio/AudioEngine.h"
#include "engine/camera/CameraManager.h"
#include "engine/core/Application.h"
#include "game/components/AudioSourceComponent.h"
#include "game/components/TransformComponent.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

void updateAudioListener(const CameraManager& cameraManager, engine::audio::AudioEngine& audio) {
    const auto& cam = cameraManager.getState();
    audio.setListenerTransform(cam.position, cam.forward, cam.up);
}

void processAudioSources(GameRegistry& registry, engine::audio::AudioEngine& audio) {
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
    auto& ctx = app.registry().ctx();
    if (ctx.contains<CameraManager*>()) {
        updateAudioListener(*ctx.get<CameraManager*>(), audio_);
    }
    processAudioSources(app.registry(), audio_);
}

void AudioListenerSystem::shutdown() {}
