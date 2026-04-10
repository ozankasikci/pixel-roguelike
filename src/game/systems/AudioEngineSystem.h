#pragma once

#include "engine/core/System.h"

namespace engine::audio {
class AudioEngine;
}

/// Thin System wrapper that ticks the AudioEngine each frame.
/// AudioEngine manages its own lifecycle (init/shutdown), so this system
/// only calls update(deltaTime) during the Gameplay phase.
class AudioEngineSystem : public System {
public:
    explicit AudioEngineSystem(engine::audio::AudioEngine& audioEngine);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    engine::audio::AudioEngine& audioEngine_;
};
