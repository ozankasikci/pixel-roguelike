#include "engine/rendering/assets/AssetCache.h"
#include "common/TestSupport.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void testFnvHash() {
    // Known FNV-1a 64-bit hash for "hello"
    // FNV-1a("hello") = 0xa430d84680aabd0b (verified against reference implementations)
    const char* input = "hello";
    uint64_t hash = AssetCache::hashBytes(input, 5);
    // Compute expected manually:
    // offset = 14695981039346656037
    // 'h' (104): hash = (14695981039346656037 ^ 104) * 1099511628211
    // 'e' (101): ...
    // 'l' (108): ...
    // 'l' (108): ...
    // 'o' (111): ...
    // Expected: 0xa430d84680aabd0b = 11831194018420276491
    constexpr uint64_t kExpectedHelloHash = 0xa430d84680aabd0bULL;
    assert(hash == kExpectedHelloHash && "FNV-1a hash of 'hello' must match known value");

    // Empty input should return offset basis
    uint64_t emptyHash = AssetCache::hashBytes("", 0);
    assert(emptyHash == 14695981039346656037ULL && "FNV-1a hash of empty input must be offset basis");

    // Different inputs produce different hashes
    uint64_t hash2 = AssetCache::hashBytes("world", 5);
    assert(hash != hash2 && "Different inputs must produce different hashes");

    std::cout << "  PASS: FNV-1a hash\n";
}

void testMeshRoundTrip() {
    const auto testDir = test_support::resetTempDirectory("test_asset_cache_mesh");
    const auto sourceFile = testDir / "test_mesh.glb";

    // Create a dummy source file to hash against
    {
        std::ofstream out(sourceFile, std::ios::binary);
        const char data[] = "dummy mesh source file content v1";
        out.write(data, sizeof(data) - 1);
    }

    // Create test interleaved vertex data (2 vertices, 11 floats each)
    std::vector<float> vertices = {
        // Vertex 0: pos(1,2,3) norm(0,1,0) uv(0.5,0.5) tan(1,0,0)
        1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        // Vertex 1: pos(4,5,6) norm(0,0,1) uv(0.25,0.75) tan(0,1,0)
        4.0f, 5.0f, 6.0f, 0.0f, 0.0f, 1.0f, 0.25f, 0.75f, 0.0f, 1.0f, 0.0f,
    };
    std::vector<uint32_t> indices = {0, 1, 0};
    glm::vec3 aabbMin(1.0f, 2.0f, 3.0f);
    glm::vec3 aabbMax(4.0f, 5.0f, 6.0f);

    // Temporarily override cwd to testDir so cacheRoot uses it
    auto originalCwd = std::filesystem::current_path();
    // Create an assets/ directory in testDir so cacheRoot finds the "project root"
    std::filesystem::create_directories(testDir / "assets");
    std::filesystem::current_path(testDir);

    // Write to cache
    AssetCache::writeMeshCache(sourceFile.string(), vertices, indices, aabbMin, aabbMax);

    // Verify cache file was created
    assert(std::filesystem::exists(testDir / ".cache" / "meshes") &&
           "Cache meshes directory must exist");

    // Read back from cache
    auto cached = AssetCache::findMeshCache(sourceFile.string());
    assert(cached.has_value() && "Cache hit expected after write");
    assert(cached->interleavedVertices.size() == vertices.size() &&
           "Vertex data size must match");
    assert(cached->indices.size() == indices.size() && "Index data size must match");

    for (size_t i = 0; i < vertices.size(); ++i) {
        assert(cached->interleavedVertices[i] == vertices[i] &&
               "Vertex data must match exactly");
    }
    for (size_t i = 0; i < indices.size(); ++i) {
        assert(cached->indices[i] == indices[i] && "Index data must match exactly");
    }
    assert(test_support::nearlyEqualVec3(cached->aabbMin, aabbMin) &&
           "AABB min must match");
    assert(test_support::nearlyEqualVec3(cached->aabbMax, aabbMax) &&
           "AABB max must match");

    // Restore cwd
    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(testDir);
    std::cout << "  PASS: mesh round-trip\n";
}

void testMeshInvalidation() {
    const auto testDir = test_support::resetTempDirectory("test_asset_cache_invalidation");
    const auto sourceFile = testDir / "test_invalidation.glb";

    // Create initial source file
    {
        std::ofstream out(sourceFile, std::ios::binary);
        const char data[] = "original content";
        out.write(data, sizeof(data) - 1);
    }

    std::filesystem::create_directories(testDir / "assets");
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(testDir);

    // Write cache with original content
    std::vector<float> vertices = {1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    std::vector<uint32_t> indices = {0};
    glm::vec3 aabbMin(1.0f, 2.0f, 3.0f);
    glm::vec3 aabbMax(1.0f, 2.0f, 3.0f);

    AssetCache::writeMeshCache(sourceFile.string(), vertices, indices, aabbMin, aabbMax);

    // Verify hit
    auto cached = AssetCache::findMeshCache(sourceFile.string());
    assert(cached.has_value() && "Should hit cache with original content");

    // Modify source file to invalidate cache
    {
        std::ofstream out(sourceFile, std::ios::binary);
        const char data[] = "modified content!!!";
        out.write(data, sizeof(data) - 1);
    }

    // Now find should miss (different hash = different filename, so no file found)
    auto stale = AssetCache::findMeshCache(sourceFile.string());
    assert(!stale.has_value() && "Should miss cache after source file modification");

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(testDir);
    std::cout << "  PASS: mesh invalidation\n";
}

void testTextureRoundTrip() {
    const auto testDir = test_support::resetTempDirectory("test_asset_cache_texture");

    std::filesystem::create_directories(testDir / "assets");
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(testDir);

    const std::string name = "test_brick_albedo";
    const uint64_t paramHash = 12345678ULL;
    const uint16_t width = 4;
    const uint16_t height = 4;
    const uint8_t channels = 4; // RGBA

    // Create test pixel data
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * channels);
    for (size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>(i % 256);
    }

    // Write to cache
    AssetCache::writeTextureCache(name, paramHash, pixels, width, height, channels);

    // Read back
    auto cached = AssetCache::findTextureCache(name, paramHash);
    assert(cached.has_value() && "Texture cache hit expected after write");
    assert(cached->width == width && "Width must match");
    assert(cached->height == height && "Height must match");
    assert(cached->channels == channels && "Channels must match");
    assert(cached->pixels.size() == pixels.size() && "Pixel data size must match");
    for (size_t i = 0; i < pixels.size(); ++i) {
        assert(cached->pixels[i] == pixels[i] && "Pixel data must match exactly");
    }

    // Test with mismatched param hash -- should miss
    auto mismatched = AssetCache::findTextureCache(name, 99999999ULL);
    assert(!mismatched.has_value() && "Should miss with different param hash");

    // Test R8 texture (single channel)
    const std::string nameR8 = "test_roughness";
    const uint8_t channelsR8 = 1;
    std::vector<uint8_t> pixelsR8(static_cast<size_t>(width) * height);
    for (size_t i = 0; i < pixelsR8.size(); ++i) {
        pixelsR8[i] = static_cast<uint8_t>((i * 17) % 256);
    }

    AssetCache::writeTextureCache(nameR8, paramHash, pixelsR8, width, height, channelsR8);
    auto cachedR8 = AssetCache::findTextureCache(nameR8, paramHash);
    assert(cachedR8.has_value() && "R8 texture cache hit expected");
    assert(cachedR8->channels == 1 && "R8 channels must be 1");
    assert(cachedR8->pixels.size() == pixelsR8.size() && "R8 pixel data size must match");

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(testDir);
    std::cout << "  PASS: texture round-trip\n";
}

} // namespace

int main() {
    std::cout << "test_asset_cache:\n";
    testFnvHash();
    testMeshRoundTrip();
    testMeshInvalidation();
    testTextureRoundTrip();
    std::cout << "All tests passed.\n";
    return 0;
}
