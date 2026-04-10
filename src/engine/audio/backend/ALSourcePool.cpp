#include "engine/audio/backend/ALSourcePool.h"

#include <AL/al.h>
#include <AL/efx.h>

#include <spdlog/spdlog.h>

#include <utility>

namespace engine::audio {

ALSourcePool::~ALSourcePool() {
    shutdown();
}

ALSourcePool::ALSourcePool(ALSourcePool&& other) noexcept
    : sources_(std::move(other.sources_)) {
}

ALSourcePool& ALSourcePool::operator=(ALSourcePool&& other) noexcept {
    if (this != &other) {
        shutdown();
        sources_ = std::move(other.sources_);
    }
    return *this;
}

bool ALSourcePool::init(uint32_t count) {
    if (!sources_.empty()) {
        spdlog::warn("[ALSourcePool] Already initialized with {} sources", sources_.size());
        return false;
    }

    sources_.resize(count, 0);
    alGenSources(static_cast<ALsizei>(count), sources_.data());

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("[ALSourcePool] alGenSources failed (error 0x{:04X})", err);
        sources_.clear();
        return false;
    }

    spdlog::info("[ALSourcePool] Allocated {} sources", count);
    return true;
}

void ALSourcePool::shutdown() {
    if (sources_.empty()) {
        return;
    }

    // Stop all sources first
    for (ALuint src : sources_) {
        alSourceStop(src);
    }

    alDeleteSources(static_cast<ALsizei>(sources_.size()), sources_.data());
    sources_.clear();

    spdlog::info("[ALSourcePool] Shutdown");
}

ALuint ALSourcePool::source(uint32_t index) const {
    if (index >= sources_.size()) {
        spdlog::error("[ALSourcePool] source({}) out of range (size={})", index, sources_.size());
        return 0;
    }
    return sources_[index];
}

void ALSourcePool::resetSource(uint32_t index) {
    if (index >= sources_.size()) {
        spdlog::error("[ALSourcePool] resetSource({}) out of range (size={})",
                      index, sources_.size());
        return;
    }

    ALuint src = sources_[index];

    alSourceStop(src);
    alSourcei(src, AL_BUFFER, 0);
    alSource3f(src, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(src, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcef(src, AL_GAIN, 1.0f);
    alSourcef(src, AL_PITCH, 1.0f);
    alSourcei(src, AL_LOOPING, AL_FALSE);
    alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
    alSourcef(src, AL_REFERENCE_DISTANCE, 1.0f);
    alSourcef(src, AL_MAX_DISTANCE, FLT_MAX);
    alSourcef(src, AL_ROLLOFF_FACTOR, 1.0f);

    // Clear direct filter (EFX) — uses 0 which is AL_FILTER_NULL
    alSourcei(src, AL_DIRECT_FILTER, 0);
    // Clear auxiliary send (slot 0)
    alSource3i(src, AL_AUXILIARY_SEND_FILTER, 0, 0, 0);
}

} // namespace engine::audio
