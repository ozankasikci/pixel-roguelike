#include "game/systems/AudioListenerSystem.h"

#include "engine/audio/AudioSystem.h"
#include "engine/camera/CameraManager.h"
#include "engine/core/Application.h"

AudioSystem& audioSystem(Application& app) {
    return *app.getService<AudioSystem*>();
}

void updateAudioListener(const CameraManager& cameraManager, AudioSystem& audio) {
    const auto& cam = cameraManager.getState();
    audio.setListenerTransform(cam.position, cam.forward, cam.up);
}

// ---------------------------------------------------------------------------
// AudioListenerSystem
// ---------------------------------------------------------------------------

AudioListenerSystem::AudioListenerSystem(AudioSystem& audio)
    : audio_(audio)
{}

void AudioListenerSystem::init(Application& /*app*/) {}

void AudioListenerSystem::update(Application& app, float /*deltaTime*/) {
    auto* camManager = app.registry().ctx().contains<CameraManager*>()
        ? app.registry().ctx().get<CameraManager*>()
        : nullptr;
    if (camManager != nullptr) {
        updateAudioListener(*camManager, audio_);
    }
}

void AudioListenerSystem::shutdown() {}
