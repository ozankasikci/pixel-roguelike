#include "engine/rendering/assets/AssetCache.h"
#include "common/TestSupport.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_set>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct ScopedCwd {
    std::filesystem::path original;
    ScopedCwd(const std::filesystem::path& testDir) : original(std::filesystem::current_path()) {
        std::filesystem::create_directories(testDir / "assets");
        std::filesystem::current_path(testDir);
    }
    ~ScopedCwd() { std::filesystem::current_path(original); }
};

void writeFile(const std::filesystem::path& path, const void* data, size_t size) {
    std::ofstream out(path, std::ios::binary);
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    writeFile(path, content.data(), content.size());
}

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return {};
    auto size = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

// Minimal interleaved vertex: 11 floats
std::vector<float> makeVertices(size_t count) {
    std::vector<float> v(count * 11);
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<float>(i) * 0.1f;
    }
    return v;
}

std::vector<uint32_t> makeIndices(size_t count) {
    std::vector<uint32_t> idx(count);
    for (size_t i = 0; i < count; ++i) idx[i] = static_cast<uint32_t>(i % 3);
    return idx;
}

std::vector<uint8_t> makePixels(uint16_t w, uint16_t h, uint8_t ch) {
    size_t n = static_cast<size_t>(w) * h * ch;
    std::vector<uint8_t> px(n);
    for (size_t i = 0; i < n; ++i) px[i] = static_cast<uint8_t>((i * 37) % 256);
    return px;
}

int passCount = 0;
int failCount = 0;

void pass(const char* name) {
    std::cout << "  PASS: " << name << "\n";
    ++passCount;
}

void fail(const char* name, const char* reason) {
    std::cout << "  FAIL: " << name << " -- " << reason << "\n";
    ++failCount;
}

#define CHECK(cond, name, reason) do { \
    if (!(cond)) { fail(name, reason); return; } \
} while (0)

// ---------------------------------------------------------------------------
// FNV-1a Hash Tests
// ---------------------------------------------------------------------------

void testFnvHashKnownValue() {
    // Reference value: FNV-1a 64-bit of "hello"
    constexpr uint64_t kExpected = 0xa430d84680aabd0bULL;
    uint64_t hash = AssetCache::hashBytes("hello", 5);
    CHECK(hash == kExpected, "fnv_known_value",
          "FNV-1a('hello') does not match reference");
    pass("fnv_known_value");
}

void testFnvHashEmptyInput() {
    constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
    uint64_t hash = AssetCache::hashBytes("", 0);
    CHECK(hash == kOffsetBasis, "fnv_empty",
          "FNV-1a('') should be offset basis");
    pass("fnv_empty");
}

void testFnvHashDifferentInputs() {
    uint64_t h1 = AssetCache::hashBytes("abc", 3);
    uint64_t h2 = AssetCache::hashBytes("abd", 3);
    uint64_t h3 = AssetCache::hashBytes("ab", 2);
    CHECK(h1 != h2, "fnv_different", "1-byte difference must produce different hash");
    CHECK(h1 != h3, "fnv_different", "different length must produce different hash");
    CHECK(h2 != h3, "fnv_different", "all three should differ");
    pass("fnv_different_inputs");
}

void testFnvHashDeterministic() {
    const char data[] = "determinism check 12345";
    uint64_t h1 = AssetCache::hashBytes(data, sizeof(data) - 1);
    uint64_t h2 = AssetCache::hashBytes(data, sizeof(data) - 1);
    CHECK(h1 == h2, "fnv_deterministic", "same input must produce same hash");
    pass("fnv_deterministic");
}

void testFnvHashSingleByte() {
    // Every single-byte input should produce a unique hash
    std::unordered_set<uint64_t> seen;
    for (int b = 0; b < 256; ++b) {
        uint8_t byte = static_cast<uint8_t>(b);
        uint64_t h = AssetCache::hashBytes(&byte, 1);
        seen.insert(h);
    }
    CHECK(seen.size() == 256, "fnv_single_byte",
          "all 256 single-byte inputs must produce distinct hashes");
    pass("fnv_single_byte_uniqueness");
}

// ---------------------------------------------------------------------------
// hashFileContents Tests
// ---------------------------------------------------------------------------

void testHashFileNonExistent() {
    uint64_t h = AssetCache::hashFileContents("/tmp/definitely_does_not_exist_9999.bin");
    CHECK(h == 0, "hash_file_nonexistent", "non-existent file should return 0");
    pass("hash_file_nonexistent");
}

void testHashFileEmpty() {
    const auto testDir = test_support::resetTempDirectory("test_cache_hash_empty");
    auto emptyFile = testDir / "empty.bin";
    writeFile(emptyFile, "", 0);

    constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
    uint64_t h = AssetCache::hashFileContents(emptyFile.string());
    CHECK(h == kOffsetBasis, "hash_file_empty",
          "empty file should hash to offset basis");

    std::filesystem::remove_all(testDir);
    pass("hash_file_empty");
}

void testHashFileMatchesHashBytes() {
    const auto testDir = test_support::resetTempDirectory("test_cache_hash_match");
    auto file = testDir / "data.bin";
    std::string content = "The quick brown fox jumps over the lazy dog";
    writeFile(file, content);

    uint64_t fileHash = AssetCache::hashFileContents(file.string());
    uint64_t bytesHash = AssetCache::hashBytes(content.data(), content.size());
    CHECK(fileHash == bytesHash, "hash_file_matches_bytes",
          "hashFileContents must equal hashBytes of same data");

    std::filesystem::remove_all(testDir);
    pass("hash_file_matches_bytes");
}

// ---------------------------------------------------------------------------
// Mesh Cache: Binary Format Correctness
// ---------------------------------------------------------------------------

void testMeshBinaryFormat() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mesh_format");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "cube.glb";
    writeFile(sourceFile, "cube data v1");

    auto verts = makeVertices(3);  // 3 vertices, 33 floats
    auto indices = makeIndices(6); // 6 indices
    glm::vec3 aabbMin(-1.0f, -2.0f, -3.0f);
    glm::vec3 aabbMax(1.0f, 2.0f, 3.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabbMin, aabbMax);

    // Find the cache file
    auto meshDir = testDir / ".cache" / "meshes";
    CHECK(std::filesystem::exists(meshDir), "mesh_binary_format",
          ".cache/meshes/ directory must exist");

    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        if (entry.path().extension() == ".bin") {
            cacheFile = entry.path();
            break;
        }
    }
    CHECK(!cacheFile.empty(), "mesh_binary_format", "cache .bin file must exist");

    auto bytes = readFileBytes(cacheFile);

    // Header: 48 bytes
    // magic(4) + version(1) + padding(3) + hash(8) + vertCount(4) + idxCount(4) + aabbMin(12) + aabbMax(12)
    size_t expectedSize = 48 + (33 * sizeof(float)) + (6 * sizeof(uint32_t));
    CHECK(bytes.size() == expectedSize, "mesh_binary_format",
          "file size must be header + vertex data + index data");

    // Check magic
    CHECK(bytes[0] == 'M' && bytes[1] == 'S' && bytes[2] == 'H' && bytes[3] == '\0',
          "mesh_binary_format", "magic must be MSH\\0");

    // Check version
    CHECK(bytes[4] == 1, "mesh_binary_format", "version must be 1");

    // Check padding is zero
    CHECK(bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 0,
          "mesh_binary_format", "padding bytes must be zero");

    // Check vertex count at offset 16 (after magic+version+padding+hash)
    uint32_t vertCount;
    std::memcpy(&vertCount, bytes.data() + 16, 4);
    CHECK(vertCount == 3, "mesh_binary_format", "vertex count must be 3");

    // Check index count at offset 20
    uint32_t idxCount;
    std::memcpy(&idxCount, bytes.data() + 20, 4);
    CHECK(idxCount == 6, "mesh_binary_format", "index count must be 6");

    // Check AABB min at offset 24
    float readMin[3];
    std::memcpy(readMin, bytes.data() + 24, 12);
    CHECK(readMin[0] == -1.0f && readMin[1] == -2.0f && readMin[2] == -3.0f,
          "mesh_binary_format", "AABB min must match");

    // Check AABB max at offset 36
    float readMax[3];
    std::memcpy(readMax, bytes.data() + 36, 12);
    CHECK(readMax[0] == 1.0f && readMax[1] == 2.0f && readMax[2] == 3.0f,
          "mesh_binary_format", "AABB max must match");

    // Check vertex data starts at offset 48 and matches
    float firstVert;
    std::memcpy(&firstVert, bytes.data() + 48, sizeof(float));
    CHECK(firstVert == verts[0], "mesh_binary_format",
          "first vertex float must match written data");

    std::filesystem::remove_all(testDir);
    pass("mesh_binary_format");
}

// ---------------------------------------------------------------------------
// Texture Cache: Binary Format Correctness
// ---------------------------------------------------------------------------

void testTextureBinaryFormat() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_format");
    ScopedCwd cwd(testDir);

    uint16_t w = 8, h = 4;
    uint8_t ch = 4;
    uint64_t paramHash = 0xDEADBEEFCAFE1234ULL;
    auto pixels = makePixels(w, h, ch);

    AssetCache::writeTextureCache("brick_albedo", paramHash, pixels, w, h, ch);

    auto texDir = testDir / ".cache" / "textures";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(texDir)) {
        if (entry.path().extension() == ".bin") {
            cacheFile = entry.path();
            break;
        }
    }
    CHECK(!cacheFile.empty(), "tex_binary_format", "cache .bin file must exist");

    auto bytes = readFileBytes(cacheFile);

    // Header: 24 bytes + pixel data
    size_t expectedSize = 24 + (static_cast<size_t>(w) * h * ch);
    CHECK(bytes.size() == expectedSize, "tex_binary_format",
          "file size must be 24-byte header + pixel data");

    // Check magic
    CHECK(bytes[0] == 'T' && bytes[1] == 'E' && bytes[2] == 'X' && bytes[3] == '\0',
          "tex_binary_format", "magic must be TEX\\0");

    // Check version
    CHECK(bytes[4] == 1, "tex_binary_format", "version must be 1");

    // Check width at offset 6
    uint16_t readW;
    std::memcpy(&readW, bytes.data() + 6, 2);
    CHECK(readW == w, "tex_binary_format", "width must match");

    // Check height at offset 8
    uint16_t readH;
    std::memcpy(&readH, bytes.data() + 8, 2);
    CHECK(readH == h, "tex_binary_format", "height must match");

    // Check channels at offset 10
    CHECK(bytes[10] == ch, "tex_binary_format", "channels must match");

    // Check param hash at offset 12
    uint64_t readHash;
    std::memcpy(&readHash, bytes.data() + 12, 8);
    CHECK(readHash == paramHash, "tex_binary_format", "param hash must match");

    // Check pixel data starts at offset 24
    CHECK(bytes[24] == pixels[0], "tex_binary_format",
          "first pixel byte must match");

    std::filesystem::remove_all(testDir);
    pass("tex_binary_format");
}

// ---------------------------------------------------------------------------
// Mesh Cache: Round-Trip Fidelity
// ---------------------------------------------------------------------------

void testMeshRoundTrip() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mesh_rt");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "model data for round-trip test");

    auto verts = makeVertices(5);
    auto indices = makeIndices(12);
    glm::vec3 aabbMin(1.5f, -0.5f, 3.14159f);
    glm::vec3 aabbMax(100.0f, 200.0f, 300.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabbMin, aabbMax);
    auto cached = AssetCache::findMeshCache(sourceFile.string());

    CHECK(cached.has_value(), "mesh_round_trip", "cache hit expected");
    CHECK(cached->interleavedVertices.size() == verts.size(), "mesh_round_trip",
          "vertex count must match");
    CHECK(cached->indices.size() == indices.size(), "mesh_round_trip",
          "index count must match");

    for (size_t i = 0; i < verts.size(); ++i) {
        CHECK(cached->interleavedVertices[i] == verts[i], "mesh_round_trip",
              "vertex data must match exactly (bitwise)");
    }
    for (size_t i = 0; i < indices.size(); ++i) {
        CHECK(cached->indices[i] == indices[i], "mesh_round_trip",
              "index data must match exactly");
    }
    CHECK(test_support::nearlyEqualVec3(cached->aabbMin, aabbMin), "mesh_round_trip",
          "AABB min must match");
    CHECK(test_support::nearlyEqualVec3(cached->aabbMax, aabbMax), "mesh_round_trip",
          "AABB max must match");

    std::filesystem::remove_all(testDir);
    pass("mesh_round_trip");
}

void testMeshRoundTripSingleVertex() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mesh_1v");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "tiny.glb";
    writeFile(sourceFile, "tiny mesh");

    std::vector<float> verts = {1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f};
    std::vector<uint32_t> indices = {0, 0, 0};
    glm::vec3 aabb(1.0f, 2.0f, 3.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);
    auto cached = AssetCache::findMeshCache(sourceFile.string());

    CHECK(cached.has_value(), "mesh_single_vertex", "cache hit expected");
    CHECK(cached->interleavedVertices.size() == 11, "mesh_single_vertex",
          "single vertex = 11 floats");
    CHECK(cached->indices.size() == 3, "mesh_single_vertex", "3 indices");

    std::filesystem::remove_all(testDir);
    pass("mesh_single_vertex");
}

void testMeshRoundTripLarge() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mesh_large");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "big.glb";
    // Write a larger source file to ensure hashing works with bigger inputs
    std::string bigContent(64 * 1024, 'X'); // 64 KB
    writeFile(sourceFile, bigContent);

    auto verts = makeVertices(1000);   // 11,000 floats
    auto indices = makeIndices(3000);  // 3,000 indices
    glm::vec3 aabbMin(-500.0f, -500.0f, -500.0f);
    glm::vec3 aabbMax(500.0f, 500.0f, 500.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabbMin, aabbMax);
    auto cached = AssetCache::findMeshCache(sourceFile.string());

    CHECK(cached.has_value(), "mesh_large", "cache hit expected for large mesh");
    CHECK(cached->interleavedVertices.size() == 11000, "mesh_large",
          "1000 vertices * 11 floats");
    CHECK(cached->indices.size() == 3000, "mesh_large", "3000 indices");

    // Spot-check a few values
    CHECK(cached->interleavedVertices[0] == verts[0], "mesh_large", "first float");
    CHECK(cached->interleavedVertices[10999] == verts[10999], "mesh_large", "last float");

    std::filesystem::remove_all(testDir);
    pass("mesh_large_round_trip");
}

// ---------------------------------------------------------------------------
// Texture Cache: Round-Trip Fidelity
// ---------------------------------------------------------------------------

void testTextureRoundTripRGBA() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_rgba");
    ScopedCwd cwd(testDir);

    uint16_t w = 16, h = 16;
    uint8_t ch = 4;
    uint64_t paramHash = 42ULL;
    auto pixels = makePixels(w, h, ch);

    AssetCache::writeTextureCache("stone_albedo", paramHash, pixels, w, h, ch);
    auto cached = AssetCache::findTextureCache("stone_albedo", paramHash);

    CHECK(cached.has_value(), "tex_rgba_round_trip", "cache hit expected");
    CHECK(cached->width == w, "tex_rgba_round_trip", "width must match");
    CHECK(cached->height == h, "tex_rgba_round_trip", "height must match");
    CHECK(cached->channels == ch, "tex_rgba_round_trip", "channels must match");
    CHECK(cached->pixels.size() == pixels.size(), "tex_rgba_round_trip", "pixel count must match");

    for (size_t i = 0; i < pixels.size(); ++i) {
        CHECK(cached->pixels[i] == pixels[i], "tex_rgba_round_trip", "pixel data mismatch");
    }

    std::filesystem::remove_all(testDir);
    pass("tex_rgba_round_trip");
}

void testTextureRoundTripR8() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_r8");
    ScopedCwd cwd(testDir);

    uint16_t w = 32, h = 32;
    uint8_t ch = 1;
    uint64_t paramHash = 99999ULL;
    auto pixels = makePixels(w, h, ch);

    AssetCache::writeTextureCache("roughness_map", paramHash, pixels, w, h, ch);
    auto cached = AssetCache::findTextureCache("roughness_map", paramHash);

    CHECK(cached.has_value(), "tex_r8_round_trip", "cache hit expected");
    CHECK(cached->channels == 1, "tex_r8_round_trip", "R8 must have 1 channel");
    CHECK(cached->pixels.size() == static_cast<size_t>(w) * h, "tex_r8_round_trip",
          "pixel count = w*h for R8");

    std::filesystem::remove_all(testDir);
    pass("tex_r8_round_trip");
}

void testTextureRoundTripNonSquare() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_nonsq");
    ScopedCwd cwd(testDir);

    uint16_t w = 64, h = 8;
    uint8_t ch = 4;
    uint64_t paramHash = 777ULL;
    auto pixels = makePixels(w, h, ch);

    AssetCache::writeTextureCache("wide_tex", paramHash, pixels, w, h, ch);
    auto cached = AssetCache::findTextureCache("wide_tex", paramHash);

    CHECK(cached.has_value(), "tex_nonsquare", "cache hit for non-square texture");
    CHECK(cached->width == 64, "tex_nonsquare", "width 64");
    CHECK(cached->height == 8, "tex_nonsquare", "height 8");
    CHECK(cached->pixels.size() == pixels.size(), "tex_nonsquare", "pixel data size");

    std::filesystem::remove_all(testDir);
    pass("tex_nonsquare_round_trip");
}

// ---------------------------------------------------------------------------
// Cache Invalidation
// ---------------------------------------------------------------------------

void testMeshInvalidationByContentChange() {
    const auto testDir = test_support::resetTempDirectory("test_cache_inval_content");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "original content v1");

    auto verts = makeVertices(2);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    // Confirm hit
    auto hit = AssetCache::findMeshCache(sourceFile.string());
    CHECK(hit.has_value(), "inval_content", "should hit before modification");

    // Modify source file
    writeFile(sourceFile, "modified content v2!!!");

    // Should miss: hash changed, cache file has different hash in name
    auto miss = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!miss.has_value(), "inval_content", "must miss after source modification");

    std::filesystem::remove_all(testDir);
    pass("mesh_invalidation_by_content");
}

void testMeshInvalidationByAppend() {
    const auto testDir = test_support::resetTempDirectory("test_cache_inval_append");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "base content");

    auto verts = makeVertices(1);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    // Append data (simulates re-export with additional geometry)
    {
        std::ofstream out(sourceFile, std::ios::binary | std::ios::app);
        const char extra[] = " extra data appended";
        out.write(extra, sizeof(extra) - 1);
    }

    auto miss = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!miss.has_value(), "inval_append", "must miss after appending to source");

    std::filesystem::remove_all(testDir);
    pass("mesh_invalidation_by_append");
}

void testMeshInvalidationRewriteCache() {
    const auto testDir = test_support::resetTempDirectory("test_cache_inval_rewrite");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "content v1");

    auto verts1 = makeVertices(2);
    auto indices1 = makeIndices(3);
    glm::vec3 aabb1(1.0f, 2.0f, 3.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts1, indices1, aabb1, aabb1);

    // Now update source file and write new cache
    writeFile(sourceFile, "content v2 longer now");
    auto verts2 = makeVertices(4);
    auto indices2 = makeIndices(6);
    glm::vec3 aabb2(10.0f, 20.0f, 30.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts2, indices2, aabb2, aabb2);

    // Should get the NEW data back
    auto cached = AssetCache::findMeshCache(sourceFile.string());
    CHECK(cached.has_value(), "inval_rewrite", "should hit with new content");
    CHECK(cached->interleavedVertices.size() == verts2.size(), "inval_rewrite",
          "should return new vertex data");
    CHECK(test_support::nearlyEqualVec3(cached->aabbMin, aabb2), "inval_rewrite",
          "should return new AABB");

    std::filesystem::remove_all(testDir);
    pass("mesh_invalidation_rewrite");
}

void testTextureInvalidationByParamHash() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_inval");
    ScopedCwd cwd(testDir);

    auto pixels = makePixels(4, 4, 4);
    AssetCache::writeTextureCache("brick", 100ULL, pixels, 4, 4, 4);

    // Hit with correct hash
    auto hit = AssetCache::findTextureCache("brick", 100ULL);
    CHECK(hit.has_value(), "tex_inval_hash", "should hit with matching hash");

    // Miss with different hash
    auto miss = AssetCache::findTextureCache("brick", 200ULL);
    CHECK(!miss.has_value(), "tex_inval_hash", "should miss with different param hash");

    // Miss with different name but same hash
    auto missName = AssetCache::findTextureCache("stone", 100ULL);
    CHECK(!missName.has_value(), "tex_inval_hash", "should miss with different name");

    std::filesystem::remove_all(testDir);
    pass("texture_invalidation_by_param_hash");
}

// ---------------------------------------------------------------------------
// Stale File Coexistence
// ---------------------------------------------------------------------------

void testStaleFilesCoexist() {
    const auto testDir = test_support::resetTempDirectory("test_cache_stale_coexist");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";

    // Write cache for version 1
    writeFile(sourceFile, "version 1");
    auto v1 = makeVertices(1);
    auto idx = makeIndices(3);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), v1, idx, aabb, aabb);

    // Write cache for version 2 (different source content, new hash = new file)
    writeFile(sourceFile, "version 2");
    auto v2 = makeVertices(2);
    AssetCache::writeMeshCache(sourceFile.string(), v2, idx, aabb, aabb);

    // Write cache for version 3
    writeFile(sourceFile, "version 3");
    auto v3 = makeVertices(3);
    AssetCache::writeMeshCache(sourceFile.string(), v3, idx, aabb, aabb);

    // Should have 3 separate cache files in meshes/
    auto meshDir = testDir / ".cache" / "meshes";
    int fileCount = 0;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        if (entry.path().extension() == ".bin") ++fileCount;
    }
    CHECK(fileCount == 3, "stale_coexist",
          "3 versions should produce 3 separate cache files");

    // Current version (3) should hit
    auto cached = AssetCache::findMeshCache(sourceFile.string());
    CHECK(cached.has_value(), "stale_coexist", "current version should hit");
    CHECK(cached->interleavedVertices.size() == v3.size(), "stale_coexist",
          "should return version 3 data");

    std::filesystem::remove_all(testDir);
    pass("stale_files_coexist");
}

// ---------------------------------------------------------------------------
// Corrupt File Handling
// ---------------------------------------------------------------------------

void testCorruptMeshTruncatedHeader() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_hdr");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "source data");

    // Write a valid cache first so we know the filename
    auto verts = makeVertices(2);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    // Find the cache file and truncate it
    auto meshDir = testDir / ".cache" / "meshes";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        cacheFile = entry.path();
        break;
    }

    // Truncate to 20 bytes (less than 48-byte header)
    auto bytes = readFileBytes(cacheFile);
    writeFile(cacheFile, bytes.data(), 20);

    auto result = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!result.has_value(), "corrupt_truncated_header",
          "truncated header must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_mesh_truncated_header");
}

void testCorruptMeshTruncatedData() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_data");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "source data for truncation test");

    auto verts = makeVertices(10);
    auto indices = makeIndices(30);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    // Find cache file and truncate after the header (keep header intact but cut vertex data)
    auto meshDir = testDir / ".cache" / "meshes";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        cacheFile = entry.path();
        break;
    }

    auto bytes = readFileBytes(cacheFile);
    // Keep header (48 bytes) + only 10 bytes of vertex data
    writeFile(cacheFile, bytes.data(), 58);

    auto result = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!result.has_value(), "corrupt_truncated_data",
          "truncated vertex data must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_mesh_truncated_data");
}

void testCorruptMeshBadMagic() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_magic");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "source data for magic test");

    auto verts = makeVertices(2);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    auto meshDir = testDir / ".cache" / "meshes";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        cacheFile = entry.path();
        break;
    }

    // Corrupt magic bytes
    auto bytes = readFileBytes(cacheFile);
    bytes[0] = 'X';
    bytes[1] = 'X';
    writeFile(cacheFile, bytes.data(), bytes.size());

    auto result = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!result.has_value(), "corrupt_bad_magic",
          "wrong magic must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_mesh_bad_magic");
}

void testCorruptMeshBadVersion() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_ver");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "source data for version test");

    auto verts = makeVertices(2);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    auto meshDir = testDir / ".cache" / "meshes";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        cacheFile = entry.path();
        break;
    }

    // Set version to 99
    auto bytes = readFileBytes(cacheFile);
    bytes[4] = 99;
    writeFile(cacheFile, bytes.data(), bytes.size());

    auto result = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!result.has_value(), "corrupt_bad_version",
          "wrong version must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_mesh_bad_version");
}

void testCorruptTextureTruncatedHeader() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_tex_hdr");
    ScopedCwd cwd(testDir);

    auto pixels = makePixels(4, 4, 4);
    uint64_t paramHash = 12345ULL;
    AssetCache::writeTextureCache("brick", paramHash, pixels, 4, 4, 4);

    auto texDir = testDir / ".cache" / "textures";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(texDir)) {
        cacheFile = entry.path();
        break;
    }

    // Truncate to 10 bytes (header is 24 bytes)
    auto bytes = readFileBytes(cacheFile);
    writeFile(cacheFile, bytes.data(), 10);

    auto result = AssetCache::findTextureCache("brick", paramHash);
    CHECK(!result.has_value(), "corrupt_tex_truncated_header",
          "truncated texture header must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_texture_truncated_header");
}

void testCorruptTextureTruncatedPixels() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_tex_px");
    ScopedCwd cwd(testDir);

    uint16_t w = 8, h = 8;
    auto pixels = makePixels(w, h, 4);
    uint64_t paramHash = 67890ULL;
    AssetCache::writeTextureCache("stone", paramHash, pixels, w, h, 4);

    auto texDir = testDir / ".cache" / "textures";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(texDir)) {
        cacheFile = entry.path();
        break;
    }

    // Keep header (24 bytes) + only 5 bytes of pixel data (should be 256)
    auto bytes = readFileBytes(cacheFile);
    writeFile(cacheFile, bytes.data(), 29);

    auto result = AssetCache::findTextureCache("stone", paramHash);
    CHECK(!result.has_value(), "corrupt_tex_truncated_pixels",
          "truncated pixel data must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_texture_truncated_pixels");
}

void testCorruptTextureGarbage() {
    const auto testDir = test_support::resetTempDirectory("test_cache_corrupt_tex_garb");
    ScopedCwd cwd(testDir);

    uint64_t paramHash = 11111ULL;
    auto pixels = makePixels(4, 4, 4);
    AssetCache::writeTextureCache("test", paramHash, pixels, 4, 4, 4);

    auto texDir = testDir / ".cache" / "textures";
    std::filesystem::path cacheFile;
    for (auto& entry : std::filesystem::directory_iterator(texDir)) {
        cacheFile = entry.path();
        break;
    }

    // Write complete garbage
    std::mt19937 rng(42);
    auto bytes = readFileBytes(cacheFile);
    for (auto& b : bytes) b = static_cast<uint8_t>(rng() % 256);
    writeFile(cacheFile, bytes.data(), bytes.size());

    // Should return nullopt (magic/version/hash mismatch)
    auto result = AssetCache::findTextureCache("test", paramHash);
    CHECK(!result.has_value(), "corrupt_tex_garbage",
          "garbage data must return nullopt");

    std::filesystem::remove_all(testDir);
    pass("corrupt_texture_garbage_data");
}

// ---------------------------------------------------------------------------
// Directory Creation
// ---------------------------------------------------------------------------

void testMeshCacheDirCreation() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mkdir_mesh");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "source");

    // .cache/ does not exist yet
    CHECK(!std::filesystem::exists(testDir / ".cache"), "mkdir_mesh",
          ".cache/ should not exist before first write");

    auto verts = makeVertices(1);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    CHECK(std::filesystem::exists(testDir / ".cache" / "meshes"), "mkdir_mesh",
          ".cache/meshes/ must be created by writeMeshCache");

    std::filesystem::remove_all(testDir);
    pass("mesh_cache_dir_creation");
}

void testTextureCacheDirCreation() {
    const auto testDir = test_support::resetTempDirectory("test_cache_mkdir_tex");
    ScopedCwd cwd(testDir);

    CHECK(!std::filesystem::exists(testDir / ".cache"), "mkdir_tex",
          ".cache/ should not exist before first write");

    auto pixels = makePixels(2, 2, 4);
    AssetCache::writeTextureCache("test", 1ULL, pixels, 2, 2, 4);

    CHECK(std::filesystem::exists(testDir / ".cache" / "textures"), "mkdir_tex",
          ".cache/textures/ must be created by writeTextureCache");

    std::filesystem::remove_all(testDir);
    pass("texture_cache_dir_creation");
}

// ---------------------------------------------------------------------------
// Idempotency
// ---------------------------------------------------------------------------

void testMeshWriteIdempotent() {
    const auto testDir = test_support::resetTempDirectory("test_cache_idempotent");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "idempotent source");

    auto verts = makeVertices(3);
    auto indices = makeIndices(6);
    glm::vec3 aabb(1.0f, 2.0f, 3.0f);

    // Write twice with identical data
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);
    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);

    // Should still produce a valid cache hit
    auto cached = AssetCache::findMeshCache(sourceFile.string());
    CHECK(cached.has_value(), "idempotent", "double-write must still hit");
    CHECK(cached->interleavedVertices.size() == verts.size(), "idempotent",
          "data must match after double write");

    // Should still be exactly one file (same hash, overwritten)
    auto meshDir = testDir / ".cache" / "meshes";
    int fileCount = 0;
    for (auto& entry : std::filesystem::directory_iterator(meshDir)) {
        if (entry.path().extension() == ".bin") ++fileCount;
    }
    CHECK(fileCount == 1, "idempotent",
          "same source should produce exactly one cache file");

    std::filesystem::remove_all(testDir);
    pass("mesh_write_idempotent");
}

// ---------------------------------------------------------------------------
// Miss on Clean State
// ---------------------------------------------------------------------------

void testMeshMissNoCacheDir() {
    const auto testDir = test_support::resetTempDirectory("test_cache_miss_nodir");
    ScopedCwd cwd(testDir);
    auto sourceFile = testDir / "model.glb";
    writeFile(sourceFile, "no cache exists yet");

    // No .cache/ directory exists
    auto result = AssetCache::findMeshCache(sourceFile.string());
    CHECK(!result.has_value(), "miss_no_cache_dir",
          "must return nullopt when cache dir doesn't exist");

    std::filesystem::remove_all(testDir);
    pass("mesh_miss_no_cache_dir");
}

void testTextureMissNoCacheDir() {
    const auto testDir = test_support::resetTempDirectory("test_cache_tex_miss_nodir");
    ScopedCwd cwd(testDir);

    auto result = AssetCache::findTextureCache("nonexistent", 42ULL);
    CHECK(!result.has_value(), "tex_miss_no_cache_dir",
          "must return nullopt when cache dir doesn't exist");

    std::filesystem::remove_all(testDir);
    pass("texture_miss_no_cache_dir");
}

// ---------------------------------------------------------------------------
// Special Characters in Names
// ---------------------------------------------------------------------------

void testMeshWithPathCharacters() {
    const auto testDir = test_support::resetTempDirectory("test_cache_path_chars");
    ScopedCwd cwd(testDir);

    // Source file in a subdirectory
    auto subdir = testDir / "models" / "doors";
    std::filesystem::create_directories(subdir);
    auto sourceFile = subdir / "double_door.glb";
    writeFile(sourceFile, "door model data");

    auto verts = makeVertices(2);
    auto indices = makeIndices(3);
    glm::vec3 aabb(0.0f);

    AssetCache::writeMeshCache(sourceFile.string(), verts, indices, aabb, aabb);
    auto cached = AssetCache::findMeshCache(sourceFile.string());

    CHECK(cached.has_value(), "path_chars",
          "source files in subdirectories must cache correctly");

    std::filesystem::remove_all(testDir);
    pass("mesh_with_path_characters");
}

} // namespace

int main() {
    std::cout << "test_asset_cache:\n";
    std::cout << "\n--- FNV-1a Hash ---\n";
    testFnvHashKnownValue();
    testFnvHashEmptyInput();
    testFnvHashDifferentInputs();
    testFnvHashDeterministic();
    testFnvHashSingleByte();

    std::cout << "\n--- hashFileContents ---\n";
    testHashFileNonExistent();
    testHashFileEmpty();
    testHashFileMatchesHashBytes();

    std::cout << "\n--- Binary Format ---\n";
    testMeshBinaryFormat();
    testTextureBinaryFormat();

    std::cout << "\n--- Mesh Round-Trip ---\n";
    testMeshRoundTrip();
    testMeshRoundTripSingleVertex();
    testMeshRoundTripLarge();

    std::cout << "\n--- Texture Round-Trip ---\n";
    testTextureRoundTripRGBA();
    testTextureRoundTripR8();
    testTextureRoundTripNonSquare();

    std::cout << "\n--- Invalidation ---\n";
    testMeshInvalidationByContentChange();
    testMeshInvalidationByAppend();
    testMeshInvalidationRewriteCache();
    testTextureInvalidationByParamHash();

    std::cout << "\n--- Stale File Coexistence ---\n";
    testStaleFilesCoexist();

    std::cout << "\n--- Corrupt File Handling ---\n";
    testCorruptMeshTruncatedHeader();
    testCorruptMeshTruncatedData();
    testCorruptMeshBadMagic();
    testCorruptMeshBadVersion();
    testCorruptTextureTruncatedHeader();
    testCorruptTextureTruncatedPixels();
    testCorruptTextureGarbage();

    std::cout << "\n--- Directory Creation ---\n";
    testMeshCacheDirCreation();
    testTextureCacheDirCreation();

    std::cout << "\n--- Idempotency ---\n";
    testMeshWriteIdempotent();

    std::cout << "\n--- Clean State Misses ---\n";
    testMeshMissNoCacheDir();
    testTextureMissNoCacheDir();

    std::cout << "\n--- Special Cases ---\n";
    testMeshWithPathCharacters();

    std::cout << "\n=============================\n";
    std::cout << "Results: " << passCount << " passed, " << failCount << " failed\n";

    if (failCount > 0) {
        std::cout << "SOME TESTS FAILED!\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}
