#include "engine/audio/backend/ALStreamPlayer.h"

#include <AL/al.h>

#include <spdlog/spdlog.h>

#include <utility>

namespace engine::audio {

ALStreamPlayer::~ALStreamPlayer() {
    destroy();
}

ALStreamPlayer::ALStreamPlayer(ALStreamPlayer&& other) noexcept
    : source_(std::exchange(other.source_, 0u)),
      buffers_(std::exchange(other.buffers_, {})),
      vorbis_(std::exchange(other.vorbis_, nullptr)),
      info_(other.info_),
      looping_(other.looping_),
      active_(std::exchange(other.active_, false)),
      gain_(other.gain_) {
}

ALStreamPlayer& ALStreamPlayer::operator=(ALStreamPlayer&& other) noexcept {
    if (this != &other) {
        destroy();
        source_ = std::exchange(other.source_, 0u);
        buffers_ = std::exchange(other.buffers_, {});
        vorbis_ = std::exchange(other.vorbis_, nullptr);
        info_ = other.info_;
        looping_ = other.looping_;
        active_ = std::exchange(other.active_, false);
        gain_ = other.gain_;
    }
    return *this;
}

bool ALStreamPlayer::play(const std::string& path, bool loop) {
    // Stop any existing playback first
    if (active_) {
        destroy();
    }

    int error = 0;
    vorbis_ = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
    if (!vorbis_) {
        spdlog::warn("[ALStreamPlayer] Failed to open OGG '{}' (error {})", path, error);
        return false;
    }

    info_ = stb_vorbis_get_info(vorbis_);
    looping_ = loop;

    // Allocate AL source and buffer ring
    alGenSources(1, &source_);
    alGenBuffers(kBufferCount, buffers_.data());

    // Non-positional: relative to listener at origin
    alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(source_, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSourcef(source_, AL_GAIN, gain_);

    // Prime all buffers
    for (int i = 0; i < kBufferCount; ++i) {
        bool reachedEnd = false;
        int written = fillBuffer(buffers_[i], reachedEnd);
        if (written > 0) {
            alSourceQueueBuffers(source_, 1, &buffers_[i]);
        }
    }

    alSourcePlay(source_);
    active_ = true;

    spdlog::debug("[ALStreamPlayer] Playing '{}' ({}Hz, {} ch, loop={})",
                  path, info_.sample_rate, info_.channels, loop);

    return true;
}

void ALStreamPlayer::stop() {
    destroy();
}

void ALStreamPlayer::pause() {
    if (active_ && source_ != 0) {
        alSourcePause(source_);
    }
}

void ALStreamPlayer::resume() {
    if (active_ && source_ != 0) {
        ALint state = AL_STOPPED;
        alGetSourcei(source_, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) {
            alSourcePlay(source_);
        }
    }
}

void ALStreamPlayer::update() {
    if (!active_ || vorbis_ == nullptr) {
        return;
    }

    ALint processed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buf = 0;
        alSourceUnqueueBuffers(source_, 1, &buf);

        bool reachedEnd = false;
        int written = fillBuffer(buf, reachedEnd);

        if (written > 0) {
            alSourceQueueBuffers(source_, 1, &buf);
        }

        if (reachedEnd && !looping_) {
            // Non-looping: drain naturally, deactivate when no buffers remain
            ALint queued = 0;
            alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
            if (queued == 0) {
                active_ = false;
                return;
            }
        }
    }

    // Underrun recovery: if the source stopped due to buffer starvation,
    // restart it while we still have queued buffers.
    if (active_) {
        ALint state = AL_STOPPED;
        alGetSourcei(source_, AL_SOURCE_STATE, &state);

        ALint queued = 0;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);

        if (state == AL_STOPPED && queued > 0) {
            spdlog::debug("[ALStreamPlayer] Underrun recovery — restarting source");
            alSourcef(source_, AL_GAIN, gain_);
            alSourcePlay(source_);
        }
    }
}

void ALStreamPlayer::setGain(float gain) {
    gain_ = gain;
    if (source_ != 0) {
        alSourcef(source_, AL_GAIN, gain_);
    }
}

bool ALStreamPlayer::isPlaying() const {
    if (source_ == 0) {
        return false;
    }
    ALint state = AL_STOPPED;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool ALStreamPlayer::isActive() const {
    return active_;
}

int ALStreamPlayer::fillBuffer(ALuint buffer, bool& reachedEnd) {
    short pcm[kPcmSamples];
    int samplesDecoded = stb_vorbis_get_samples_short_interleaved(
        vorbis_, info_.channels, pcm, kPcmSamples);

    if (samplesDecoded == 0) {
        reachedEnd = true;
        if (looping_) {
            stb_vorbis_seek_start(vorbis_);
            samplesDecoded = stb_vorbis_get_samples_short_interleaved(
                vorbis_, info_.channels, pcm, kPcmSamples);
        }
    }

    if (samplesDecoded > 0) {
        ALenum fmt = (info_.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        alBufferData(buffer, fmt, pcm,
                     static_cast<ALsizei>(samplesDecoded * info_.channels * sizeof(short)),
                     static_cast<ALsizei>(info_.sample_rate));
    }

    return samplesDecoded;
}

void ALStreamPlayer::destroy() {
    if (!active_ && source_ == 0) {
        return;
    }

    if (source_ != 0) {
        alSourceStop(source_);

        // Unqueue all buffers before deleting
        ALint queued = 0;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint buf = 0;
            alSourceUnqueueBuffers(source_, 1, &buf);
        }

        alDeleteSources(1, &source_);
        source_ = 0;
    }

    // Delete buffers that were allocated
    bool anyBuffers = false;
    for (auto b : buffers_) {
        if (b != 0) {
            anyBuffers = true;
            break;
        }
    }
    if (anyBuffers) {
        alDeleteBuffers(kBufferCount, buffers_.data());
        buffers_.fill(0);
    }

    if (vorbis_ != nullptr) {
        stb_vorbis_close(vorbis_);
        vorbis_ = nullptr;
    }

    active_ = false;
}

} // namespace engine::audio
