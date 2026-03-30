#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct CachedMeshData {
    std::vector<float> interleavedVertices; // 11 floats per vertex (pos3 + norm3 + uv2 + tan3)
    std::vector<uint32_t> indices;
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
};

struct CachedTextureData {
    std::vector<uint8_t> pixels;
    uint16_t width;
    uint16_t height;
    uint8_t channels; // 4 = RGBA, 1 = R8
};

class AssetCache {
public:
    // Look up a cached mesh by source file path. Returns nullopt on miss or stale cache.
    static std::optional<CachedMeshData> findMeshCache(const std::string& filepath);

    // Look up a cached mesh using separate source file path (for hashing) and cache label (for
    // filename). Use this for multi-mesh FBX where the cache key contains a virtual "#submesh"
    // suffix that cannot be opened as a file.
    static std::optional<CachedMeshData> findMeshCache(const std::string& sourceFilePath,
                                                        const std::string& cacheLabel);

    // Write a processed mesh to the disk cache.
    static void writeMeshCache(const std::string& filepath,
                               const std::vector<float>& interleavedVertices,
                               const std::vector<uint32_t>& indices,
                               const glm::vec3& aabbMin,
                               const glm::vec3& aabbMax);

    // Write a processed mesh to the disk cache using separate source file path (for hashing) and
    // cache label (for filename). Use this for multi-mesh FBX submeshes.
    static void writeMeshCache(const std::string& sourceFilePath,
                               const std::string& cacheLabel,
                               const std::vector<float>& interleavedVertices,
                               const std::vector<uint32_t>& indices,
                               const glm::vec3& aabbMin,
                               const glm::vec3& aabbMax);

    // Look up a cached texture by name and parameter hash. Returns nullopt on miss.
    static std::optional<CachedTextureData> findTextureCache(const std::string& name,
                                                             uint64_t paramHash);

    // Write a processed texture to the disk cache.
    static void writeTextureCache(const std::string& name,
                                  uint64_t paramHash,
                                  const std::vector<uint8_t>& pixels,
                                  uint16_t width,
                                  uint16_t height,
                                  uint8_t channels);

    // Hash the contents of a file using FNV-1a 64-bit.
    static uint64_t hashFileContents(const std::string& filepath);

    // FNV-1a 64-bit hash of arbitrary bytes.
    static uint64_t hashBytes(const void* data, size_t length);

private:
    static std::filesystem::path cacheRoot();
    static std::string toHexString(uint64_t value);
};
