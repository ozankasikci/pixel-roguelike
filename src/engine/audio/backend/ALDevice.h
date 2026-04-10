#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include <string>

namespace engine::audio {

/// Capabilities queried from the OpenAL device after context creation.
struct DeviceCapabilities {
    int maxSources = 0;
    bool hasEFX = false;
    int maxAuxSends = 0;
    bool hasHRTF = false;
};

/// RAII wrapper for an ALCdevice and ALCcontext pair.
/// Opens the default device, creates a context, and sets the distance model.
/// Move-only, non-copyable.
class ALDevice {
public:
    ALDevice() = default;
    ~ALDevice();

    ALDevice(const ALDevice&) = delete;
    ALDevice& operator=(const ALDevice&) = delete;
    ALDevice(ALDevice&& other) noexcept;
    ALDevice& operator=(ALDevice&& other) noexcept;

    /// Open the default audio device and create a context.
    /// Sets AL_INVERSE_DISTANCE_CLAMPED as the distance model.
    /// Returns true on success.
    bool open();

    /// Close the context and device, releasing all resources.
    void close();

    /// Returns true if the device and context are valid.
    bool isOpen() const;

    /// Query device capabilities (EFX, HRTF, source limits).
    DeviceCapabilities queryCapabilities() const;

    /// Raw handle access (for advanced callers).
    ALCdevice* device() const {
        return device_;
    }

    ALCcontext* context() const {
        return context_;
    }

private:
    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
};

} // namespace engine::audio
