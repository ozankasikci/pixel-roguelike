#pragma once
#include "engine/core/System.h"

#include "engine/ecs/GameRegistry.h"

class Application;

namespace engine::audio {
class AudioEngine;
}

// Free-function to sync audio listener position from CameraManager.
class CameraManager;
void updateAudioListener(const CameraManager& cameraManager, engine::audio::AudioEngine& audio);

// Free-function to process AudioSourceComponent triggers via AudioEngine.
void processAudioSources(GameRegistry& registry, engine::audio::AudioEngine& audio);

// System wrapper for registration via addSystem (matches PlayerMovementSystem/CameraSystem pattern).
// Holds a reference to AudioEngine obtained at construction time.
class AudioListenerSystem : public System {
public:
    explicit AudioListenerSystem(engine::audio::AudioEngine& audio);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    engine::audio::AudioEngine& audio_;
};
