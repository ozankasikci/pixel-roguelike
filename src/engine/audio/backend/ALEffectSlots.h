#pragma once

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/efx.h>

#include <cstdint>
#include <vector>

namespace engine::audio {

/// Parameters for configuring a reverb effect (maps to EAX Reverb or standard Reverb).
struct ReverbParams {
    float density = 1.0f;
    float diffusion = 1.0f;
    float gain = 0.32f;
    float gainHF = 0.89f;
    float decayTime = 1.49f;
    float decayHFRatio = 0.83f;
    float reflectionsGain = 0.05f;
    float reflectionsDelay = 0.007f;
    float lateReverbGain = 1.26f;
    float lateReverbDelay = 0.011f;
    float airAbsorption = 0.994f;
    float roomRolloff = 0.0f;
};

/// Manages OpenAL EFX auxiliary effect slots, effects, and filters.
/// Provides reverb setup and low-pass filter creation for occlusion.
/// Move-only, non-copyable.
class ALEffectSlots {
public:
    ALEffectSlots() = default;
    ~ALEffectSlots();

    ALEffectSlots(const ALEffectSlots&) = delete;
    ALEffectSlots& operator=(const ALEffectSlots&) = delete;
    ALEffectSlots(ALEffectSlots&& other) noexcept;
    ALEffectSlots& operator=(ALEffectSlots&& other) noexcept;

    /// Allocate effect slots and effects. Tries EAX Reverb first, falls back
    /// to standard Reverb. Returns true if at least one slot was created.
    bool init(uint32_t requestedSlots = 1);

    /// Delete all effect slots, effects, and filters.
    void shutdown();

    /// Apply reverb parameters to the effect at the given slot index.
    void setReverb(uint32_t slotIndex, const ReverbParams& params);

    /// Return the auxiliary effect slot handle for connecting to sources
    /// via AL_AUXILIARY_SEND_FILTER. Returns 0 if index is out of range.
    ALuint slotHandle(uint32_t index) const;

    /// Create a low-pass filter for occlusion. Returns the filter handle,
    /// or 0 on failure. Caller is responsible for calling destroyFilter().
    ALuint createFilter();

    /// Set low-pass filter parameters (gain and high-frequency gain).
    void setFilterParams(ALuint filter, float gain, float gainHF);

    /// Delete a filter previously created by createFilter().
    void destroyFilter(ALuint filter);

    /// Number of allocated slots.
    uint32_t slotCount() const {
        return static_cast<uint32_t>(slots_.size());
    }

    /// Whether EAX Reverb is being used (vs standard Reverb fallback).
    bool isEAXReverb() const {
        return useEAX_;
    }

private:
    std::vector<ALuint> slots_;
    std::vector<ALuint> effects_;
    bool useEAX_ = false;
};

} // namespace engine::audio
