#include "engine/audio/backend/ALBufferCache.h"

#include <AL/al.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace engine::audio {

namespace {

/// Determine the OpenAL format enum from channel count and bit depth.
ALenum wavFormat(int channels, int bitsPerSample) {
    if (channels == 1 && bitsPerSample == 8) return AL_FORMAT_MONO8;
    if (channels == 1 && bitsPerSample == 16) return AL_FORMAT_MONO16;
    if (channels == 2 && bitsPerSample == 8) return AL_FORMAT_STEREO8;
    if (channels == 2 && bitsPerSample == 16) return AL_FORMAT_STEREO16;
    return 0;
}

} // anonymous namespace

ALBufferCache::~ALBufferCache() {
    clear();
}

ALBufferCache::ALBufferCache(ALBufferCache&& other) noexcept
    : cache_(std::move(other.cache_)) {
}

ALBufferCache& ALBufferCache::operator=(ALBufferCache&& other) noexcept {
    if (this != &other) {
        clear();
        cache_ = std::move(other.cache_);
    }
    return *this;
}

ALuint ALBufferCache::getOrLoad(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        return it->second;
    }

    ALuint buf = loadWav(path);
    if (buf != 0) {
        cache_[path] = buf;
    }
    return buf;
}

void ALBufferCache::preload(const std::vector<std::string>& paths, const std::string& baseDir) {
    for (const auto& name : paths) {
        std::string fullPath = baseDir;
        if (!fullPath.empty() && fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += name;
        getOrLoad(fullPath);
    }
}

bool ALBufferCache::contains(const std::string& path) const {
    return cache_.find(path) != cache_.end();
}

void ALBufferCache::clear() {
    for (auto& [path, buf] : cache_) {
        alDeleteBuffers(1, &buf);
    }
    cache_.clear();
}

ALuint ALBufferCache::loadWav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("[ALBufferCache] Cannot open WAV file '{}'", path);
        return 0;
    }

    // --- Parse RIFF/WAVE header ---
    char riffMagic[4];
    uint32_t chunkSize;
    char waveMagic[4];

    file.read(riffMagic, 4);
    file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
    file.read(waveMagic, 4);

    if (std::strncmp(riffMagic, "RIFF", 4) != 0 || std::strncmp(waveMagic, "WAVE", 4) != 0) {
        spdlog::warn("[ALBufferCache] '{}' is not a valid WAV file", path);
        return 0;
    }

    // --- Parse fmt chunk ---
    char fmtId[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;

    file.read(fmtId, 4);
    file.read(reinterpret_cast<char*>(&fmtSize), sizeof(fmtSize));
    file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
    file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
    file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
    file.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
    file.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
    file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

    // Skip any extra fmt bytes
    if (fmtSize > 16) {
        file.seekg(fmtSize - 16, std::ios::cur);
    }

    // --- Find data chunk (skip non-data chunks) ---
    char dataId[4];
    uint32_t dataSize;

    file.read(dataId, 4);
    file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

    // Skip non-data chunks (e.g., LIST, fact) to find the actual data chunk
    while (std::strncmp(dataId, "data", 4) != 0 && file.good()) {
        file.seekg(dataSize, std::ios::cur);
        file.read(dataId, 4);
        file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
    }

    if (!file.good()) {
        spdlog::warn("[ALBufferCache] '{}' has no data chunk", path);
        return 0;
    }

    // --- Validate format ---
    if (audioFormat != 1) {
        spdlog::warn("[ALBufferCache] '{}' is not PCM (audioFormat={})", path, audioFormat);
        return 0;
    }
    if (bitsPerSample != 8 && bitsPerSample != 16) {
        spdlog::warn("[ALBufferCache] '{}' has unsupported bit depth ({})", path, bitsPerSample);
        return 0;
    }

    ALenum fmt = wavFormat(channels, bitsPerSample);
    if (fmt == 0) {
        spdlog::warn("[ALBufferCache] '{}' unsupported format ({} ch, {} bit)",
                     path, channels, bitsPerSample);
        return 0;
    }

    // --- Read PCM data ---
    std::vector<char> pcmData(dataSize);
    file.read(pcmData.data(), dataSize);

    if (!file.good() && !file.eof()) {
        spdlog::warn("[ALBufferCache] '{}' truncated read (expected {} bytes)", path, dataSize);
        return 0;
    }

    // --- Upload to OpenAL ---
    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, fmt, pcmData.data(), static_cast<ALsizei>(dataSize),
                 static_cast<ALsizei>(sampleRate));

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::warn("[ALBufferCache] alBufferData failed for '{}' (error 0x{:04X})", path, err);
        alDeleteBuffers(1, &buf);
        return 0;
    }

    spdlog::debug("[ALBufferCache] Loaded '{}' ({} ch, {} Hz, {} bit, {} bytes)",
                  path, channels, sampleRate, bitsPerSample, dataSize);

    return buf;
}

} // namespace engine::audio
