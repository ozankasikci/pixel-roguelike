#include "engine/audio/AudioEngine.h"

#include <AL/al.h>

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#include "engine/audio/backend/ALBufferCache.h"
#include "engine/audio/backend/ALDevice.h"
#include "engine/audio/backend/ALEffectSlots.h"
#include "engine/audio/backend/ALSourcePool.h"
#include "engine/audio/backend/ALStreamPlayer.h"
#include "engine/audio/mix/ReverbManager.h"
#include "engine/audio/mix/Voice.h"
#include "engine/audio/mix/VoiceManager.h"

namespace engine::audio {

// ---------------------------------------------------------------------------
// Pimpl
// ---------------------------------------------------------------------------

struct AudioEngine::Impl {
    AudioEngineConfig config;

    // Backend layer
    ALDevice device;
    ALSourcePool sourcePool;
    ALBufferCache bufferCache;
    ALEffectSlots effectSlots;
    ALStreamPlayer musicStream;
    ALStreamPlayer ambientStream;

    // Mix layer
    BusGraph busGraph;
    VoiceManager voiceManager;
    OcclusionProcessor occlusionProcessor;
    ReverbManager reverbManager;

    // Events layer
    EventRegistry eventRegistry;

    // Listener state
    glm::vec3 listenerPos{0.0f};

    bool initialized = false;

    explicit Impl(const AudioEngineConfig& cfg)
        : config(cfg)
        , voiceManager(cfg.voicePoolSize)
        , occlusionProcessor(cfg.occlusionQueryRate) {
    }
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AudioEngine::AudioEngine(const AudioEngineConfig& config)
    : impl_(std::make_unique<Impl>(config)) {
}

AudioEngine::~AudioEngine() {
    if (impl_ && impl_->initialized) {
        shutdown();
    }
}

bool AudioEngine::init() {
    if (impl_->initialized) {
        spdlog::warn("[AudioEngine] Already initialized");
        return true;
    }

    // 1. Open device
    if (!impl_->device.open()) {
        spdlog::error("[AudioEngine] Failed to open AL device");
        return false;
    }
    spdlog::info("[AudioEngine] AL device opened");

    auto caps = impl_->device.queryCapabilities();
    spdlog::info("[AudioEngine] Device caps: maxSources={}, EFX={}, maxAuxSends={}, HRTF={}",
                 caps.maxSources, caps.hasEFX, caps.maxAuxSends, caps.hasHRTF);

    // 2. Init source pool
    uint32_t poolSize = static_cast<uint32_t>(impl_->config.voicePoolSize);
    if (!impl_->sourcePool.init(poolSize)) {
        spdlog::error("[AudioEngine] Failed to init source pool (size={})", poolSize);
        impl_->device.close();
        return false;
    }
    spdlog::info("[AudioEngine] Source pool initialized (size={})", poolSize);

    // 3. Init EFX slots (if available)
    if (caps.hasEFX) {
        uint32_t slots = static_cast<uint32_t>(impl_->config.reverbSlots);
        if (impl_->effectSlots.init(slots)) {
            spdlog::info("[AudioEngine] EFX slots initialized (count={}, eax={})",
                         impl_->effectSlots.slotCount(), impl_->effectSlots.isEAXReverb());
        } else {
            spdlog::warn("[AudioEngine] EFX slot init failed; reverb disabled");
        }
    }

    // 4. Load event registry
    if (!impl_->config.eventDefPath.empty()) {
        if (impl_->eventRegistry.loadFromFile(impl_->config.eventDefPath)) {
            auto names = impl_->eventRegistry.eventNames();
            spdlog::info("[AudioEngine] Loaded {} sound events", names.size());
        } else {
            spdlog::warn("[AudioEngine] Could not load event defs from '{}'",
                         impl_->config.eventDefPath);
        }
    }

    // 5. Preload WAV files referenced by events
    auto soundPaths = impl_->eventRegistry.allSoundPaths();
    if (!soundPaths.empty()) {
        impl_->bufferCache.preload(soundPaths, impl_->config.audioBasePath);
        spdlog::info("[AudioEngine] Preloaded {} sound files", impl_->bufferCache.size());
    }

    // 6. Set up default reverb presets
    impl_->reverbManager.addPreset("None", ReverbParams{});
    impl_->reverbManager.setPreset("None");

    // Small room preset
    ReverbParams smallRoom;
    smallRoom.decayTime = 0.8f;
    smallRoom.reflectionsGain = 0.1f;
    smallRoom.lateReverbGain = 0.6f;
    impl_->reverbManager.addPreset("SmallRoom", smallRoom);

    // Large hall preset
    ReverbParams largeHall;
    largeHall.decayTime = 3.2f;
    largeHall.reflectionsGain = 0.15f;
    largeHall.reflectionsDelay = 0.02f;
    largeHall.lateReverbGain = 1.8f;
    largeHall.lateReverbDelay = 0.04f;
    impl_->reverbManager.addPreset("LargeHall", largeHall);

    // Corridor preset
    ReverbParams corridor;
    corridor.decayTime = 1.5f;
    corridor.diffusion = 0.6f;
    corridor.reflectionsGain = 0.2f;
    corridor.lateReverbGain = 1.0f;
    impl_->reverbManager.addPreset("Corridor", corridor);

    impl_->initialized = true;
    spdlog::info("[AudioEngine] Initialization complete");
    return true;
}

void AudioEngine::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    spdlog::info("[AudioEngine] Shutting down...");

    // Stop streams
    impl_->musicStream.stop();
    impl_->ambientStream.stop();

    // Reset all sources
    for (uint32_t i = 0; i < impl_->sourcePool.size(); ++i) {
        impl_->sourcePool.resetSource(i);
    }

    // Shutdown subsystems
    impl_->effectSlots.shutdown();
    impl_->sourcePool.shutdown();
    impl_->bufferCache.clear();
    impl_->device.close();

    impl_->initialized = false;
    spdlog::info("[AudioEngine] Shutdown complete");
}

void AudioEngine::stopAll() {
    if (!impl_->initialized) return;
    impl_->voiceManager.stopAll();
    for (uint32_t i = 0; i < impl_->sourcePool.size(); ++i) {
        impl_->sourcePool.resetSource(i);
    }
}

// ---------------------------------------------------------------------------
// Play / Stop
// ---------------------------------------------------------------------------

VoiceHandle AudioEngine::play(const std::string& eventName, const glm::vec3& position,
                              float volume, float pitch) {
    if (!impl_->initialized) {
        return VoiceHandle{};
    }

    const SoundEventDef* def = impl_->eventRegistry.find(eventName);
    if (!def) {
        spdlog::warn("[AudioEngine] Unknown event '{}'", eventName);
        return VoiceHandle{};
    }

    // Check concurrency limit
    if (impl_->voiceManager.countByEvent(eventName) >= def->maxInstances) {
        return VoiceHandle{};
    }

    // Pick a sound variation
    int soundIdx = def->pickSound();
    if (soundIdx < 0 || soundIdx >= static_cast<int>(def->sounds.size())) {
        return VoiceHandle{};
    }

    // Resolve the full path and load buffer
    std::string fullPath = impl_->config.audioBasePath + "/" + def->sounds[soundIdx];
    ALuint buffer = impl_->bufferCache.getOrLoad(fullPath);
    if (buffer == 0) {
        spdlog::warn("[AudioEngine] Failed to load '{}'", fullPath);
        return VoiceHandle{};
    }

    // Apply event-level randomization
    float finalVolume = volume * def->randomVolume();
    float finalPitch = pitch * def->randomPitch();

    constexpr uint8_t kDefaultPriority = 128;
    return impl_->voiceManager.spawn(eventName, position, finalVolume, finalPitch, kDefaultPriority,
                                     def->busName, def->is3D, false, def->refDistance,
                                     def->maxDistance, static_cast<uint32_t>(buffer));
}

VoiceHandle AudioEngine::play(const std::string& eventName) {
    return play(eventName, glm::vec3{0.0f}, 1.0f, 1.0f);
}

VoiceHandle AudioEngine::playLooping(const std::string& eventName, const glm::vec3& position,
                                     float volume, float pitch) {
    if (!impl_->initialized) {
        return VoiceHandle{};
    }

    const SoundEventDef* def = impl_->eventRegistry.find(eventName);
    if (!def) {
        spdlog::warn("[AudioEngine] Unknown event '{}'", eventName);
        return VoiceHandle{};
    }

    if (impl_->voiceManager.countByEvent(eventName) >= def->maxInstances) {
        return VoiceHandle{};
    }

    int soundIdx = def->pickSound();
    if (soundIdx < 0 || soundIdx >= static_cast<int>(def->sounds.size())) {
        return VoiceHandle{};
    }

    std::string fullPath = impl_->config.audioBasePath + "/" + def->sounds[soundIdx];
    ALuint buffer = impl_->bufferCache.getOrLoad(fullPath);
    if (buffer == 0) {
        spdlog::warn("[AudioEngine] Failed to load '{}'", fullPath);
        return VoiceHandle{};
    }

    float finalVolume = volume * def->randomVolume();
    float finalPitch = pitch * def->randomPitch();

    constexpr uint8_t kDefaultPriority = 128;
    return impl_->voiceManager.spawn(eventName, position, finalVolume, finalPitch, kDefaultPriority,
                                     def->busName, def->is3D, true, def->refDistance,
                                     def->maxDistance, static_cast<uint32_t>(buffer));
}

void AudioEngine::stop(VoiceHandle handle, float fadeTime) {
    if (!impl_->initialized) {
        return;
    }
    impl_->voiceManager.stop(handle, fadeTime);
}

void AudioEngine::updateVoicePosition(VoiceHandle handle, const glm::vec3& position) {
    if (!impl_->initialized) {
        return;
    }
    impl_->voiceManager.updatePosition(handle, position);
}

// ---------------------------------------------------------------------------
// Streaming
// ---------------------------------------------------------------------------

void AudioEngine::playMusic(const std::string& path, bool loop) {
    if (!impl_->initialized) {
        return;
    }
    impl_->musicStream.stop();
    if (!impl_->musicStream.play(path, loop)) {
        spdlog::warn("[AudioEngine] Failed to play music '{}'", path);
    }
}

void AudioEngine::stopMusic() {
    if (!impl_->initialized) {
        return;
    }
    impl_->musicStream.stop();
}

void AudioEngine::playAmbient(const std::string& path, bool loop) {
    if (!impl_->initialized) {
        return;
    }
    impl_->ambientStream.stop();
    if (!impl_->ambientStream.play(path, loop)) {
        spdlog::warn("[AudioEngine] Failed to play ambient '{}'", path);
    }
}

void AudioEngine::stopAmbient() {
    if (!impl_->initialized) {
        return;
    }
    impl_->ambientStream.stop();
}

// ---------------------------------------------------------------------------
// Listener / Buses / Reverb
// ---------------------------------------------------------------------------

void AudioEngine::setListenerTransform(const glm::vec3& position, const glm::vec3& forward,
                                       const glm::vec3& up) {
    if (!impl_->initialized) {
        return;
    }
    impl_->listenerPos = position;

    alListener3f(AL_POSITION, position.x, position.y, position.z);

    ALfloat orientation[6] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioEngine::setBusVolume(const std::string& busName, float volume) {
    impl_->busGraph.setVolume(busName, volume);
}

void AudioEngine::setBusMute(const std::string& busName, bool mute) {
    impl_->busGraph.setMute(busName, mute);
}

void AudioEngine::setReverbPreset(const std::string& presetName, float transitionTime) {
    if (!impl_->reverbManager.hasPreset(presetName)) {
        spdlog::warn("[AudioEngine] Unknown reverb preset '{}'", presetName);
        return;
    }

    if (transitionTime <= 0.0f) {
        impl_->reverbManager.setPreset(presetName);
    } else {
        impl_->reverbManager.beginTransition(presetName, transitionTime);
    }

    // Apply immediately if not transitioning (or apply initial state)
    if (impl_->effectSlots.slotCount() > 0) {
        impl_->effectSlots.setReverb(0, impl_->reverbManager.currentParams());
    }
}

void AudioEngine::setRaycastFunc(RaycastFunc func) {
    impl_->occlusionProcessor.setRaycastFunc(std::move(func));
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void AudioEngine::update(float deltaTime) {
    if (!impl_->initialized) {
        return;
    }

    // 1. Update voice manager (scoring, virtualization, source assignment)
    impl_->voiceManager.update(impl_->listenerPos, impl_->busGraph, deltaTime);

    // 2. Update occlusion
    impl_->occlusionProcessor.update(impl_->voiceManager.voices(), impl_->listenerPos, deltaTime);

    // 3. Update reverb transition
    if (impl_->reverbManager.isTransitioning()) {
        impl_->reverbManager.updateTransition(deltaTime);
        if (impl_->effectSlots.slotCount() > 0) {
            impl_->effectSlots.setReverb(0, impl_->reverbManager.currentParams());
        }
    }

    // 4. Sync voices to AL sources
    syncVoicesToSources();

    // 5. Update streams
    impl_->musicStream.update();
    impl_->ambientStream.update();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

BusGraph& AudioEngine::busGraph() {
    return impl_->busGraph;
}

EventRegistry& AudioEngine::eventRegistry() {
    return impl_->eventRegistry;
}

// ---------------------------------------------------------------------------
// Internal: sync voice state to AL sources
// ---------------------------------------------------------------------------

void AudioEngine::syncVoicesToSources() {
    for (auto& voice : impl_->voiceManager.voices()) {
        if (voice.sourceIndex < 0) {
            continue;
        }
        if (voice.state != VoiceState::Playing &&
            voice.state != VoiceState::Stopping) {
            continue;
        }

        uint32_t idx = static_cast<uint32_t>(voice.sourceIndex);
        if (idx >= impl_->sourcePool.size()) {
            continue;
        }

        ALuint src = impl_->sourcePool.source(idx);

        // Set buffer (only if source has no buffer assigned yet)
        ALint currentBuf = 0;
        alGetSourcei(src, AL_BUFFER, &currentBuf);
        if (static_cast<ALuint>(currentBuf) != voice.bufferHandle) {
            alSourceStop(src);
            alSourcei(src, AL_BUFFER, static_cast<ALint>(voice.bufferHandle));
        }

        // Position
        if (voice.is3D) {
            alSource3f(src, AL_POSITION, voice.position.x, voice.position.y, voice.position.z);
            alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
            alSourcef(src, AL_REFERENCE_DISTANCE, voice.refDistance);
            alSourcef(src, AL_MAX_DISTANCE, voice.maxDistance);
        } else {
            alSource3f(src, AL_POSITION, 0.0f, 0.0f, 0.0f);
            alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
        }

        // Gain = voice volume * bus effective volume * (1 - occlusion) * fadeVolume
        float busVol = impl_->busGraph.effectiveVolume(voice.busName);
        float occlusionAtten = 1.0f - (voice.occlusion * 0.8f);
        float gain = voice.volume * busVol * occlusionAtten;
        if (voice.state == VoiceState::Stopping) {
            gain *= voice.fadeVolume;
        }
        alSourcef(src, AL_GAIN, gain);

        // Pitch
        alSourcef(src, AL_PITCH, voice.pitch);

        // Looping
        alSourcei(src, AL_LOOPING, voice.looping ? AL_TRUE : AL_FALSE);

        // Connect reverb send (slot 0) if available
        if (impl_->effectSlots.slotCount() > 0 && voice.is3D) {
            ALuint slot = impl_->effectSlots.slotHandle(0);
            alSource3i(src, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(slot), 0, AL_FILTER_NULL);
        }

        // Check AL source state
        ALint alState = 0;
        alGetSourcei(src, AL_SOURCE_STATE, &alState);

        // Check if a non-looping source has finished playing (one-shot completion)
        if (alState == AL_STOPPED && !voice.looping &&
            voice.state == VoiceState::Playing && voice.elapsedTime > 0.05f) {
            voice.state = VoiceState::Stopped;
            continue;
        }

        // Start playback if not already playing
        if (alState != AL_PLAYING) {
            alSourcePlay(src);
        }
    }
}

} // namespace engine::audio
