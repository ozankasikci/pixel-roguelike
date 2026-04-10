#pragma once

#include <AL/al.h>

#include <cstdint>
#include <vector>

namespace engine::audio {

/// Pre-allocated pool of OpenAL source handles.
/// Sources are allocated at init and reused via index. Move-only, non-copyable.
class ALSourcePool {
public:
    static constexpr uint32_t kDefaultPoolSize = 32;

    ALSourcePool() = default;
    ~ALSourcePool();

    ALSourcePool(const ALSourcePool&) = delete;
    ALSourcePool& operator=(const ALSourcePool&) = delete;
    ALSourcePool(ALSourcePool&& other) noexcept;
    ALSourcePool& operator=(ALSourcePool&& other) noexcept;

    /// Allocate N OpenAL sources. Returns true on success.
    bool init(uint32_t count = kDefaultPoolSize);

    /// Delete all sources and clear the pool.
    void shutdown();

    /// Return the AL source handle at the given index.
    /// Index must be in [0, size()).
    ALuint source(uint32_t index) const;

    /// Stop a source, clear its buffer assignment, and reset properties
    /// (position, gain, pitch, looping, direct filter) to defaults.
    void resetSource(uint32_t index);

    /// Number of sources in the pool.
    uint32_t size() const {
        return static_cast<uint32_t>(sources_.size());
    }

private:
    std::vector<ALuint> sources_;
};

} // namespace engine::audio
