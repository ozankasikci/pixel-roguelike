#pragma once

#include <AL/al.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace engine::audio {

/// Caches WAV and OGG files as OpenAL buffers, loading each path at most once.
/// WAV parser handles RIFF/WAVE, PCM-only, 8/16-bit, mono/stereo.
/// OGG parser uses stb_vorbis to fully decode Vorbis files into PCM.
/// Move-only, non-copyable.
class ALBufferCache {
public:
    ALBufferCache() = default;
    ~ALBufferCache();

    ALBufferCache(const ALBufferCache&) = delete;
    ALBufferCache& operator=(const ALBufferCache&) = delete;
    ALBufferCache(ALBufferCache&& other) noexcept;
    ALBufferCache& operator=(ALBufferCache&& other) noexcept;

    /// Load the WAV or OGG at `path` if not already cached. Returns the AL
    /// buffer handle, or 0 on failure. File type is detected by extension.
    ALuint getOrLoad(const std::string& path);

    /// Batch-load a list of audio file names relative to `baseDir`.
    void preload(const std::vector<std::string>& paths, const std::string& baseDir);

    /// Returns true if the path is already cached.
    bool contains(const std::string& path) const;

    /// Delete all cached AL buffers.
    void clear();

    /// Number of cached entries.
    std::size_t size() const {
        return cache_.size();
    }

private:
    /// Parse a WAV file from disk and upload to an AL buffer.
    /// Returns 0 on failure.
    ALuint loadWav(const std::string& path);

    /// Decode an OGG Vorbis file from disk and upload to an AL buffer.
    /// Returns 0 on failure.
    ALuint loadOgg(const std::string& path);

    std::unordered_map<std::string, ALuint> cache_;
};

} // namespace engine::audio
