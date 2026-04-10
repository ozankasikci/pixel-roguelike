#pragma once

#include <cstdint>
#include <functional>

namespace engine::audio {

struct VoiceHandle {
    uint32_t id = 0;
    uint32_t generation = 0;

    VoiceHandle() = default;

    explicit VoiceHandle(uint32_t id, uint32_t gen = 0)
        : id(id), generation(gen) {
    }

    bool valid() const {
        return id != 0;
    }

    bool operator==(const VoiceHandle& other) const {
        return id == other.id && generation == other.generation;
    }

    bool operator!=(const VoiceHandle& other) const {
        return !(*this == other);
    }
};

} // namespace engine::audio

template <>
struct std::hash<engine::audio::VoiceHandle> {
    std::size_t operator()(const engine::audio::VoiceHandle& h) const noexcept {
        uint64_t combined = (static_cast<uint64_t>(h.id) << 32) | h.generation;
        return std::hash<uint64_t>{}(combined);
    }
};
