#include "engine/audio/AudioSystem.h"
#include "engine/core/Application.h"

// OpenAL Soft headers — intentionally confined to this .cpp (pimpl hides from header)
#include <AL/al.h>
#include <AL/alc.h>

// stb_vorbis — header-only declarations only; implementation compiled separately
// as part of engine_audio target via CMake source file inclusion.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------

static constexpr int kSourcePoolSize   = 16;
static constexpr int kStreamBufferCount = 4;
static constexpr int kStreamPcmSamples = 65536; // per-buffer PCM short samples

// ---------------------------------------------------------------------------
// Helper: clamp volume to [0, 1]
// ---------------------------------------------------------------------------

static float clampVolume(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

// ---------------------------------------------------------------------------
// StreamState — file-scope so anonymous-namespace helpers can reference it
// without touching AudioSystem::Impl's private access.
// ---------------------------------------------------------------------------

struct StreamState {
    ALuint source = 0;
    std::array<ALuint, kStreamBufferCount> buffers{};
    stb_vorbis* vorbis  = nullptr;
    stb_vorbis_info info{};
    bool looping = false;
    bool active  = false;
};

// ---------------------------------------------------------------------------
// AudioSystem::Impl
// ---------------------------------------------------------------------------

struct AudioSystem::Impl {
    // --- OpenAL device/context ---
    ALCdevice*  device  = nullptr;
    ALCcontext* context = nullptr;

    // --- SFX buffer registry ---
    std::unordered_map<uint32_t, ALuint> soundBuffers;
    uint32_t nextHandle = 1;

    // --- Fire-and-forget SFX source pool ---
    std::array<ALuint, kSourcePoolSize> sourcePool{};
    int sourcePoolIndex = 0;

    // --- Streaming state (music + ambient) ---
    StreamState music;
    StreamState ambient;

    // --- Volume categories ---
    float masterVolume = 1.0f;
    float sfxVolume    = 1.0f;
    float musicVolume  = 1.0f;
    float ambientVolume = 1.0f;
};

// ---------------------------------------------------------------------------
// Internal helpers (file-scope, not in the class)
// ---------------------------------------------------------------------------

namespace {

// Fill a streaming buffer with decoded PCM from the vorbis stream.
// Returns the number of samples written; 0 means end of stream.
int fillVorbisBuffer(stb_vorbis* vorbis, stb_vorbis_info info,
                     ALuint buffer, bool looping, bool& reachedEnd) {
    short pcm[kStreamPcmSamples];
    int samplesDecoded = stb_vorbis_get_samples_short_interleaved(
        vorbis, info.channels, pcm, kStreamPcmSamples);

    if (samplesDecoded == 0) {
        reachedEnd = true;
        if (looping) {
            stb_vorbis_seek_start(vorbis);
            samplesDecoded = stb_vorbis_get_samples_short_interleaved(
                vorbis, info.channels, pcm, kStreamPcmSamples);
        }
    }

    if (samplesDecoded > 0) {
        ALenum fmt = (info.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        alBufferData(buffer, fmt,
                     pcm,
                     static_cast<ALsizei>(samplesDecoded * info.channels * sizeof(short)),
                     static_cast<ALsizei>(info.sample_rate));
    }

    return samplesDecoded;
}

// Refill a stream each frame. Handles the 4-buffer ring queue and underrun
// recovery (Research Pitfall 2).
void refillStream(StreamState& s, float effectiveGain) {
    if (!s.active || s.vorbis == nullptr) return;

    ALint processed = 0;
    alGetSourcei(s.source, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buf = 0;
        alSourceUnqueueBuffers(s.source, 1, &buf);

        bool reachedEnd = false;
        int written = fillVorbisBuffer(s.vorbis, s.info, buf, s.looping, reachedEnd);

        if (written > 0) {
            alSourceQueueBuffers(s.source, 1, &buf);
        }

        if (reachedEnd && !s.looping) {
            // Non-looping stream: drain naturally, deactivate when done
            ALint queued = 0;
            alGetSourcei(s.source, AL_BUFFERS_QUEUED, &queued);
            if (queued == 0) {
                s.active = false;
            }
        }
    }

    // Underrun recovery: if the source stopped due to buffer starvation,
    // restart it if still active and has queued buffers.
    if (s.active) {
        ALint state = AL_STOPPED;
        alGetSourcei(s.source, AL_SOURCE_STATE, &state);

        ALint queued = 0;
        alGetSourcei(s.source, AL_BUFFERS_QUEUED, &queued);

        if (state == AL_STOPPED && queued > 0) {
            alSourcef(s.source, AL_GAIN, effectiveGain);
            alSourcePlay(s.source);
        }
    }
}

// Clean up a StreamState: stop source, unqueue all buffers, delete AL objects,
// close the vorbis decoder.
void destroyStream(StreamState& s) {
    if (!s.active && s.source == 0) return;

    if (s.source != 0) {
        alSourceStop(s.source);

        // Unqueue all buffers before deleting source
        ALint queued = 0;
        alGetSourcei(s.source, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint buf = 0;
            alSourceUnqueueBuffers(s.source, 1, &buf);
        }

        alDeleteSources(1, &s.source);
        s.source = 0;
    }

    // Only delete buffers that were allocated (non-zero)
    bool anyBuffers = false;
    for (auto b : s.buffers) { if (b != 0) { anyBuffers = true; break; } }
    if (anyBuffers) {
        alDeleteBuffers(kStreamBufferCount, s.buffers.data());
        s.buffers.fill(0);
    }

    if (s.vorbis != nullptr) {
        stb_vorbis_close(s.vorbis);
        s.vorbis = nullptr;
    }

    s.active = false;
}

// Prime and start a streaming source from a vorbis file path.
bool startStream(StreamState& s, const std::string& path,
                 bool loop, float effectiveGain) {
    int error = 0;
    s.vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
    if (!s.vorbis) {
        spdlog::warn("[AudioSystem] Failed to open OGG file '{}' (error {})", path, error);
        return false;
    }

    s.info    = stb_vorbis_get_info(s.vorbis);
    s.looping = loop;

    // Allocate AL source and buffer ring
    alGenSources(1, &s.source);
    alGenBuffers(kStreamBufferCount, s.buffers.data());

    // Non-positional: relative to listener at origin
    alSourcei(s.source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(s.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSourcef(s.source, AL_GAIN, effectiveGain);

    // Prime all 4 buffers
    for (int i = 0; i < kStreamBufferCount; ++i) {
        bool reachedEnd = false;
        int written = fillVorbisBuffer(s.vorbis, s.info, s.buffers[i], loop, reachedEnd);
        if (written > 0) {
            alSourceQueueBuffers(s.source, 1, &s.buffers[i]);
        }
    }

    alSourcePlay(s.source);
    s.active = true;
    return true;
}

// Determine OpenAL format from channel count and bit depth
ALenum wavFormat(int channels, int bitsPerSample) {
    if (channels == 1 && bitsPerSample == 8)  return AL_FORMAT_MONO8;
    if (channels == 1 && bitsPerSample == 16) return AL_FORMAT_MONO16;
    if (channels == 2 && bitsPerSample == 8)  return AL_FORMAT_STEREO8;
    if (channels == 2 && bitsPerSample == 16) return AL_FORMAT_STEREO16;
    return 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// AudioSystem public implementation
// ---------------------------------------------------------------------------

AudioSystem::AudioSystem() = default;
AudioSystem::~AudioSystem() = default;

void AudioSystem::init(Application& /*app*/) {
    initDevice();
}

void AudioSystem::initDevice() {
    impl_ = std::make_unique<Impl>();

    impl_->device = alcOpenDevice(nullptr); // default device
    if (!impl_->device) {
        spdlog::error("[AudioSystem] alcOpenDevice failed — no audio output available");
        return;
    }

    impl_->context = alcCreateContext(impl_->device, nullptr);
    if (!impl_->context) {
        spdlog::error("[AudioSystem] alcCreateContext failed");
        alcCloseDevice(impl_->device);
        impl_->device = nullptr;
        return;
    }

    alcMakeContextCurrent(impl_->context);

    // Pre-allocate SFX source pool
    alGenSources(kSourcePoolSize, impl_->sourcePool.data());

    // 3D distance model (per D-14)
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    spdlog::info("[AudioSystem] Initialized (OpenAL Soft)");
}

void AudioSystem::update(Application& /*app*/, float /*deltaTime*/) {
    updateStreaming();
}

void AudioSystem::updateStreaming() {
    if (!impl_) return;

    refillStream(impl_->music,   impl_->masterVolume * impl_->musicVolume);
    refillStream(impl_->ambient, impl_->masterVolume * impl_->ambientVolume);
}

void AudioSystem::shutdown() {
    if (!impl_) return;

    destroyStream(impl_->music);
    destroyStream(impl_->ambient);

    alDeleteSources(kSourcePoolSize, impl_->sourcePool.data());

    for (auto& [handle, buf] : impl_->soundBuffers) {
        alDeleteBuffers(1, &buf);
    }
    impl_->soundBuffers.clear();

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(impl_->context);
    alcCloseDevice(impl_->device);
    impl_->context = nullptr;
    impl_->device  = nullptr;

    impl_.reset();

    spdlog::info("[AudioSystem] Shutdown");
}

// ---------------------------------------------------------------------------
// SFX API
// ---------------------------------------------------------------------------

uint32_t AudioSystem::loadSound(const std::string& path) {
    if (!impl_) return 0;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("[AudioSystem] loadSound: cannot open '{}'", path);
        return 0;
    }

    // --- Parse 44-byte WAV header ---
    // RIFF chunk
    char riffMagic[4];
    uint32_t chunkSize;
    char waveMagic[4];
    // fmt sub-chunk
    char fmtId[4];
    uint32_t fmtSize;
    uint16_t audioFormat;   // must be 1 (PCM)
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    // data sub-chunk
    char dataId[4];
    uint32_t dataSize;

    file.read(riffMagic, 4);
    file.read(reinterpret_cast<char*>(&chunkSize),     sizeof(chunkSize));
    file.read(waveMagic, 4);
    file.read(fmtId, 4);
    file.read(reinterpret_cast<char*>(&fmtSize),       sizeof(fmtSize));
    file.read(reinterpret_cast<char*>(&audioFormat),   sizeof(audioFormat));
    file.read(reinterpret_cast<char*>(&channels),      sizeof(channels));
    file.read(reinterpret_cast<char*>(&sampleRate),    sizeof(sampleRate));
    file.read(reinterpret_cast<char*>(&byteRate),      sizeof(byteRate));
    file.read(reinterpret_cast<char*>(&blockAlign),    sizeof(blockAlign));
    file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

    // Skip any extra fmt bytes
    if (fmtSize > 16) {
        file.seekg(fmtSize - 16, std::ios::cur);
    }

    file.read(dataId, 4);
    file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

    // Validate
    if (std::strncmp(riffMagic, "RIFF", 4) != 0 || std::strncmp(waveMagic, "WAVE", 4) != 0) {
        spdlog::warn("[AudioSystem] loadSound: '{}' is not a valid WAV file", path);
        return 0;
    }
    if (audioFormat != 1) {
        spdlog::warn("[AudioSystem] loadSound: '{}' is not PCM (audioFormat={})", path, audioFormat);
        return 0;
    }
    if (bitsPerSample != 8 && bitsPerSample != 16) {
        spdlog::warn("[AudioSystem] loadSound: '{}' has unsupported bit depth ({})", path, bitsPerSample);
        return 0;
    }

    ALenum fmt = wavFormat(channels, bitsPerSample);
    if (fmt == 0) {
        spdlog::warn("[AudioSystem] loadSound: '{}' has unsupported channel/bit combination ({} ch, {} bit)",
                     path, channels, bitsPerSample);
        return 0;
    }

    // Read PCM data
    std::vector<char> pcmData(dataSize);
    file.read(pcmData.data(), dataSize);

    // Upload to OpenAL
    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, fmt, pcmData.data(), static_cast<ALsizei>(dataSize),
                 static_cast<ALsizei>(sampleRate));

    if (alGetError() != AL_NO_ERROR) {
        spdlog::warn("[AudioSystem] loadSound: alBufferData failed for '{}'", path);
        alDeleteBuffers(1, &buf);
        return 0;
    }

    uint32_t handle = impl_->nextHandle++;
    impl_->soundBuffers[handle] = buf;
    return handle;
}

void AudioSystem::playSound(uint32_t handle, const glm::vec3& position,
                            float volume, float pitch,
                            float refDistance, float maxDistance) {
    if (!impl_) return;

    auto it = impl_->soundBuffers.find(handle);
    if (it == impl_->soundBuffers.end()) {
        spdlog::warn("[AudioSystem] playSound: unknown handle {}", handle);
        return;
    }

    // Round-robin source selection from pool
    ALuint source = impl_->sourcePool[impl_->sourcePoolIndex % kSourcePoolSize];
    impl_->sourcePoolIndex = (impl_->sourcePoolIndex + 1) % kSourcePoolSize;

    alSourceStop(source); // reclaim if busy

    float effectiveGain = clampVolume(volume) * impl_->masterVolume * impl_->sfxVolume;

    alSourcei(source,  AL_BUFFER,           static_cast<ALint>(it->second));
    alSource3f(source, AL_POSITION,         position.x, position.y, position.z);
    alSourcef(source,  AL_GAIN,             effectiveGain);
    alSourcef(source,  AL_PITCH,            pitch);
    alSourcef(source,  AL_REFERENCE_DISTANCE, refDistance);
    alSourcef(source,  AL_MAX_DISTANCE,     maxDistance);
    alSourcei(source,  AL_SOURCE_RELATIVE,  AL_FALSE); // 3D positioned
    alSourcei(source,  AL_LOOPING,          AL_FALSE);

    alSourcePlay(source);
}

// ---------------------------------------------------------------------------
// Music streaming
// ---------------------------------------------------------------------------

void AudioSystem::playMusic(const std::string& path, bool loop) {
    if (!impl_) return;

    if (impl_->music.active) {
        stopMusic();
    }

    float gain = impl_->masterVolume * impl_->musicVolume;
    if (!startStream(impl_->music, path, loop, gain)) {
        spdlog::warn("[AudioSystem] playMusic: failed to start stream for '{}'", path);
    }
}

void AudioSystem::stopMusic() {
    if (!impl_) return;
    destroyStream(impl_->music);
}

bool AudioSystem::isMusicPlaying() const {
    if (!impl_) return false;
    return impl_->music.active;
}

// ---------------------------------------------------------------------------
// Ambient streaming
// ---------------------------------------------------------------------------

void AudioSystem::playAmbient(const std::string& path, bool loop) {
    if (!impl_) return;

    if (impl_->ambient.active) {
        stopAmbient();
    }

    float gain = impl_->masterVolume * impl_->ambientVolume;
    if (!startStream(impl_->ambient, path, loop, gain)) {
        spdlog::warn("[AudioSystem] playAmbient: failed to start stream for '{}'", path);
    }
}

void AudioSystem::stopAmbient() {
    if (!impl_) return;
    destroyStream(impl_->ambient);
}

bool AudioSystem::isAmbientPlaying() const {
    if (!impl_) return false;
    return impl_->ambient.active;
}

// ---------------------------------------------------------------------------
// Volume controls
// ---------------------------------------------------------------------------

void AudioSystem::setMasterVolume(float v) {
    if (!impl_) return;
    impl_->masterVolume = clampVolume(v);
    // Immediately update active stream gains
    if (impl_->music.active) {
        alSourcef(impl_->music.source, AL_GAIN,
                  impl_->masterVolume * impl_->musicVolume);
    }
    if (impl_->ambient.active) {
        alSourcef(impl_->ambient.source, AL_GAIN,
                  impl_->masterVolume * impl_->ambientVolume);
    }
}

void AudioSystem::setSfxVolume(float v) {
    if (!impl_) return;
    impl_->sfxVolume = clampVolume(v);
    // SFX sources are fire-and-forget; new plays will pick up the new volume
}

void AudioSystem::setMusicVolume(float v) {
    if (!impl_) return;
    impl_->musicVolume = clampVolume(v);
    if (impl_->music.active) {
        alSourcef(impl_->music.source, AL_GAIN,
                  impl_->masterVolume * impl_->musicVolume);
    }
}

void AudioSystem::setAmbientVolume(float v) {
    if (!impl_) return;
    impl_->ambientVolume = clampVolume(v);
    if (impl_->ambient.active) {
        alSourcef(impl_->ambient.source, AL_GAIN,
                  impl_->masterVolume * impl_->ambientVolume);
    }
}

float AudioSystem::masterVolume()  const { return impl_ ? impl_->masterVolume  : 1.0f; }
float AudioSystem::sfxVolume()     const { return impl_ ? impl_->sfxVolume     : 1.0f; }
float AudioSystem::musicVolume()   const { return impl_ ? impl_->musicVolume   : 1.0f; }
float AudioSystem::ambientVolume() const { return impl_ ? impl_->ambientVolume : 1.0f; }

// ---------------------------------------------------------------------------
// Listener transform
// ---------------------------------------------------------------------------

void AudioSystem::setListenerTransform(const glm::vec3& position,
                                        const glm::vec3& forward,
                                        const glm::vec3& up) {
    if (!impl_) return;

    alListener3f(AL_POSITION, position.x, position.y, position.z);

    float ori[6] = {
        forward.x, forward.y, forward.z,
        up.x,      up.y,      up.z
    };
    alListenerfv(AL_ORIENTATION, ori);
}
