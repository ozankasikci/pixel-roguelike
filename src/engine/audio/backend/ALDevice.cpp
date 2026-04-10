#include "engine/audio/backend/ALDevice.h"

#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <spdlog/spdlog.h>

#include <utility>

namespace engine::audio {

ALDevice::~ALDevice() {
    close();
}

ALDevice::ALDevice(ALDevice&& other) noexcept
    : device_(std::exchange(other.device_, nullptr)),
      context_(std::exchange(other.context_, nullptr)) {
}

ALDevice& ALDevice::operator=(ALDevice&& other) noexcept {
    if (this != &other) {
        close();
        device_ = std::exchange(other.device_, nullptr);
        context_ = std::exchange(other.context_, nullptr);
    }
    return *this;
}

bool ALDevice::open() {
    if (device_) {
        spdlog::warn("[ALDevice] Already open — call close() first");
        return false;
    }

    device_ = alcOpenDevice(nullptr);
    if (!device_) {
        spdlog::error("[ALDevice] alcOpenDevice failed — no audio output available");
        return false;
    }

    context_ = alcCreateContext(device_, nullptr);
    if (!context_) {
        spdlog::error("[ALDevice] alcCreateContext failed");
        alcCloseDevice(device_);
        device_ = nullptr;
        return false;
    }

    if (!alcMakeContextCurrent(context_)) {
        spdlog::error("[ALDevice] alcMakeContextCurrent failed");
        alcDestroyContext(context_);
        alcCloseDevice(device_);
        context_ = nullptr;
        device_ = nullptr;
        return false;
    }

    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    const char* deviceName = alcGetString(device_, ALC_DEVICE_SPECIFIER);
    spdlog::info("[ALDevice] Opened device: {}", deviceName ? deviceName : "unknown");

    return true;
}

void ALDevice::close() {
    if (!device_) {
        return;
    }

    alcMakeContextCurrent(nullptr);

    if (context_) {
        alcDestroyContext(context_);
        context_ = nullptr;
    }

    alcCloseDevice(device_);
    device_ = nullptr;

    spdlog::info("[ALDevice] Closed");
}

bool ALDevice::isOpen() const {
    return device_ != nullptr && context_ != nullptr;
}

DeviceCapabilities ALDevice::queryCapabilities() const {
    DeviceCapabilities caps;

    if (!isOpen()) {
        return caps;
    }

    // Query mono source limit as proxy for max concurrent sources
    ALCint monoSources = 0;
    alcGetIntegerv(device_, ALC_MONO_SOURCES, 1, &monoSources);
    caps.maxSources = static_cast<int>(monoSources);

    // Check EFX extension
    caps.hasEFX = alcIsExtensionPresent(device_, ALC_EXT_EFX_NAME) == ALC_TRUE;

    if (caps.hasEFX) {
        ALCint auxSends = 0;
        alcGetIntegerv(device_, ALC_MAX_AUXILIARY_SENDS, 1, &auxSends);
        caps.maxAuxSends = static_cast<int>(auxSends);
    }

    // Check HRTF extension
    caps.hasHRTF = alcIsExtensionPresent(device_, "ALC_SOFT_HRTF") == ALC_TRUE;

    spdlog::info("[ALDevice] Capabilities: maxSources={}, EFX={}, auxSends={}, HRTF={}",
                 caps.maxSources, caps.hasEFX, caps.maxAuxSends, caps.hasHRTF);

    return caps;
}

} // namespace engine::audio
