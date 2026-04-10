#pragma once

#include <AL/al.h>

#include <array>
#include <string>

// stb_vorbis declarations only (implementation compiled separately by CMake)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace engine::audio {

/// OGG Vorbis streaming player using a double-buffered ring of 4 AL buffers.
/// Each buffer holds up to 65536 PCM samples. Call update() every frame to
/// refill processed buffers. Move-only, non-copyable.
class ALStreamPlayer {
public:
    static constexpr int kBufferCount = 4;
    static constexpr int kPcmSamples = 65536;

    ALStreamPlayer() = default;
    ~ALStreamPlayer();

    ALStreamPlayer(const ALStreamPlayer&) = delete;
    ALStreamPlayer& operator=(const ALStreamPlayer&) = delete;
    ALStreamPlayer(ALStreamPlayer&& other) noexcept;
    ALStreamPlayer& operator=(ALStreamPlayer&& other) noexcept;

    /// Open the OGG file, allocate source + buffers, prime all 4, and start.
    /// Returns true on success.
    bool play(const std::string& path, bool loop = true);

    /// Stop playback and release all AL resources.
    void stop();

    /// Pause the source (can resume later).
    void pause();

    /// Resume from pause.
    void resume();

    /// Refill processed buffers from the vorbis decoder.
    /// Call once per frame. Handles underrun recovery.
    void update();

    /// Set playback gain [0, 1].
    void setGain(float gain);

    /// True if the AL source is in the AL_PLAYING state.
    bool isPlaying() const;

    /// True if the stream is logically active (playing or paused, not stopped).
    bool isActive() const;

private:
    /// Fill one AL buffer with decoded PCM from the vorbis stream.
    /// Returns the number of samples written (0 = end of stream).
    int fillBuffer(ALuint buffer, bool& reachedEnd);

    /// Tear down all AL objects and the vorbis decoder.
    void destroy();

    ALuint source_ = 0;
    std::array<ALuint, kBufferCount> buffers_{};
    stb_vorbis* vorbis_ = nullptr;
    stb_vorbis_info info_{};
    bool looping_ = false;
    bool active_ = false;
    float gain_ = 1.0f;
};

} // namespace engine::audio
