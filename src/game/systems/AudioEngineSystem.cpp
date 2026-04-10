#include "game/systems/AudioEngineSystem.h"

#include "engine/audio/AudioEngine.h"

AudioEngineSystem::AudioEngineSystem(engine::audio::AudioEngine& audioEngine)
    : audioEngine_(audioEngine) {}

void AudioEngineSystem::init(Application& /*app*/) {}

void AudioEngineSystem::update(Application& /*app*/, float deltaTime) {
    audioEngine_.update(deltaTime);
}

void AudioEngineSystem::shutdown() {}
