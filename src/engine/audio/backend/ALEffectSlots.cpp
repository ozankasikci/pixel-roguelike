#include "engine/audio/backend/ALEffectSlots.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/efx.h>

#include <spdlog/spdlog.h>

#include <utility>

namespace engine::audio {

ALEffectSlots::~ALEffectSlots() {
    shutdown();
}

ALEffectSlots::ALEffectSlots(ALEffectSlots&& other) noexcept
    : slots_(std::move(other.slots_)),
      effects_(std::move(other.effects_)),
      useEAX_(other.useEAX_) {
}

ALEffectSlots& ALEffectSlots::operator=(ALEffectSlots&& other) noexcept {
    if (this != &other) {
        shutdown();
        slots_ = std::move(other.slots_);
        effects_ = std::move(other.effects_);
        useEAX_ = other.useEAX_;
    }
    return *this;
}

bool ALEffectSlots::init(uint32_t requestedSlots) {
    if (!slots_.empty()) {
        spdlog::warn("[ALEffectSlots] Already initialized");
        return false;
    }

    // Allocate auxiliary effect slots
    slots_.resize(requestedSlots, 0);
    alGenAuxiliaryEffectSlots(static_cast<ALsizei>(requestedSlots), slots_.data());

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("[ALEffectSlots] alGenAuxiliaryEffectSlots failed (error 0x{:04X})", err);
        slots_.clear();
        return false;
    }

    // Allocate one effect per slot
    effects_.resize(requestedSlots, 0);
    alGenEffects(static_cast<ALsizei>(requestedSlots), effects_.data());

    err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("[ALEffectSlots] alGenEffects failed (error 0x{:04X})", err);
        alDeleteAuxiliaryEffectSlots(static_cast<ALsizei>(slots_.size()), slots_.data());
        slots_.clear();
        effects_.clear();
        return false;
    }

    // Try EAX Reverb first, fall back to standard Reverb
    useEAX_ = false;
    for (uint32_t i = 0; i < requestedSlots; ++i) {
        alEffecti(effects_[i], AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        if (alGetError() == AL_NO_ERROR) {
            if (i == 0) {
                useEAX_ = true;
            }
        } else {
            // EAX not supported — fall back to standard reverb
            alEffecti(effects_[i], AL_EFFECT_TYPE, AL_EFFECT_REVERB);
            if (alGetError() != AL_NO_ERROR) {
                spdlog::error("[ALEffectSlots] Neither EAX nor standard reverb supported");
                shutdown();
                return false;
            }
            if (i == 0) {
                useEAX_ = false;
            }
        }

        // Attach effect to slot
        alAuxiliaryEffectSloti(slots_[i], AL_EFFECTSLOT_EFFECT,
                              static_cast<ALint>(effects_[i]));
    }

    spdlog::info("[ALEffectSlots] Initialized {} slot(s), reverb type: {}",
                 requestedSlots, useEAX_ ? "EAX" : "standard");

    return true;
}

void ALEffectSlots::shutdown() {
    if (slots_.empty()) {
        return;
    }

    // Detach effects from slots first
    for (ALuint slot : slots_) {
        alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
    }

    if (!effects_.empty()) {
        alDeleteEffects(static_cast<ALsizei>(effects_.size()), effects_.data());
        effects_.clear();
    }

    alDeleteAuxiliaryEffectSlots(static_cast<ALsizei>(slots_.size()), slots_.data());
    slots_.clear();

    spdlog::info("[ALEffectSlots] Shutdown");
}

void ALEffectSlots::setReverb(uint32_t slotIndex, const ReverbParams& params) {
    if (slotIndex >= effects_.size()) {
        spdlog::error("[ALEffectSlots] setReverb({}) out of range (count={})",
                      slotIndex, effects_.size());
        return;
    }

    ALuint effect = effects_[slotIndex];

    if (useEAX_) {
        alEffectf(effect, AL_EAXREVERB_DENSITY, params.density);
        alEffectf(effect, AL_EAXREVERB_DIFFUSION, params.diffusion);
        alEffectf(effect, AL_EAXREVERB_GAIN, params.gain);
        alEffectf(effect, AL_EAXREVERB_GAINHF, params.gainHF);
        alEffectf(effect, AL_EAXREVERB_DECAY_TIME, params.decayTime);
        alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, params.decayHFRatio);
        alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, params.reflectionsGain);
        alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, params.reflectionsDelay);
        alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, params.lateReverbGain);
        alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, params.lateReverbDelay);
        alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, params.airAbsorption);
        alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, params.roomRolloff);
    } else {
        alEffectf(effect, AL_REVERB_DENSITY, params.density);
        alEffectf(effect, AL_REVERB_DIFFUSION, params.diffusion);
        alEffectf(effect, AL_REVERB_GAIN, params.gain);
        alEffectf(effect, AL_REVERB_GAINHF, params.gainHF);
        alEffectf(effect, AL_REVERB_DECAY_TIME, params.decayTime);
        alEffectf(effect, AL_REVERB_DECAY_HFRATIO, params.decayHFRatio);
        alEffectf(effect, AL_REVERB_REFLECTIONS_GAIN, params.reflectionsGain);
        alEffectf(effect, AL_REVERB_REFLECTIONS_DELAY, params.reflectionsDelay);
        alEffectf(effect, AL_REVERB_LATE_REVERB_GAIN, params.lateReverbGain);
        alEffectf(effect, AL_REVERB_LATE_REVERB_DELAY, params.lateReverbDelay);
        alEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF, params.airAbsorption);
        alEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, params.roomRolloff);
    }

    // Re-attach the updated effect to the slot
    alAuxiliaryEffectSloti(slots_[slotIndex], AL_EFFECTSLOT_EFFECT,
                          static_cast<ALint>(effect));

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::warn("[ALEffectSlots] setReverb error (0x{:04X})", err);
    }
}

ALuint ALEffectSlots::slotHandle(uint32_t index) const {
    if (index >= slots_.size()) {
        spdlog::error("[ALEffectSlots] slotHandle({}) out of range (count={})",
                      index, slots_.size());
        return 0;
    }
    return slots_[index];
}

ALuint ALEffectSlots::createFilter() {
    ALuint filter = 0;
    alGenFilters(1, &filter);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("[ALEffectSlots] alGenFilters failed (error 0x{:04X})", err);
        return 0;
    }

    alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
    err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("[ALEffectSlots] Failed to set lowpass filter type (error 0x{:04X})", err);
        alDeleteFilters(1, &filter);
        return 0;
    }

    // Default: fully open (no attenuation)
    alFilterf(filter, AL_LOWPASS_GAIN, 1.0f);
    alFilterf(filter, AL_LOWPASS_GAINHF, 1.0f);

    return filter;
}

void ALEffectSlots::setFilterParams(ALuint filter, float gain, float gainHF) {
    if (filter == 0) {
        return;
    }
    alFilterf(filter, AL_LOWPASS_GAIN, gain);
    alFilterf(filter, AL_LOWPASS_GAINHF, gainHF);
}

void ALEffectSlots::destroyFilter(ALuint filter) {
    if (filter != 0) {
        alDeleteFilters(1, &filter);
    }
}

} // namespace engine::audio
