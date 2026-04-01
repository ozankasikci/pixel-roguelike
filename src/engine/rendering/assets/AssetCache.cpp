#include "engine/rendering/assets/AssetCache.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

#pragma pack(push, 1)
struct MeshCacheHeader {
    char magic[4];       // "MSH\0"
    uint8_t version;     // 1
    uint8_t padding[3];  // zero
    uint64_t contentHash;
    uint32_t vertexCount;
    uint32_t indexCount;
    float aabbMin[3];
    float aabbMax[3];
};
// Total: 4 + 1 + 3 + 8 + 4 + 4 + 12 + 12 = 48 bytes

struct TextureCacheHeader {
    char magic[4];       // "TEX\0"
    uint8_t version;     // 1
    uint8_t padding1;    // zero
    uint16_t width;
    uint16_t height;
    uint8_t channels;    // 4 = RGBA, 1 = R8
    uint8_t padding2;    // zero
    uint64_t paramHash;
    uint32_t reserved;   // zero
};
// Total: 4 + 1 + 1 + 2 + 2 + 1 + 1 + 8 + 4 = 24 bytes
#pragma pack(pop)

static_assert(sizeof(MeshCacheHeader) == 48, "MeshCacheHeader must be 48 bytes");
static_assert(sizeof(TextureCacheHeader) == 24, "TextureCacheHeader must be 24 bytes");

} // namespace

uint64_t AssetCache::hashBytes(const void* data, size_t length) {
    uint64_t hash = kFnvOffsetBasis;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t AssetCache::hashFileContents(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::warn("AssetCache: cannot open file for hashing: '{}'", filepath);
        return 0;
    }

    const auto fileSize = file.tellg();
    if (fileSize <= 0) {
        return kFnvOffsetBasis;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return hashBytes(buffer.data(), buffer.size());
}

std::string AssetCache::sanitizeCacheName(const std::string& name) {
    std::string safe;
    safe.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.') {
            safe += c;
        } else {
            safe += '_';
        }
    }
    // Truncate long names but append a hash of the full name to prevent collisions.
    // Previous approach truncated to 120 chars without a hash, causing different cache
    // keys (e.g. _file_albedo vs _file_ao) to collide when they shared a long prefix.
    constexpr size_t kMaxPrefix = 80;
    if (safe.size() > kMaxPrefix) {
        const uint64_t nameHash = hashBytes(name.data(), name.size());
        safe = safe.substr(0, kMaxPrefix) + "_" + toHexString(nameHash);
    }
    return safe;
}

std::filesystem::path AssetCache::cacheRoot() {
    // Walk up from cwd to find project root (where assets/ exists)
    auto current = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (std::filesystem::exists(current / "assets")) {
            return current / ".cache";
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    // Fallback: use cwd
    return std::filesystem::current_path() / ".cache";
}

std::string AssetCache::toHexString(uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

std::optional<CachedMeshData> AssetCache::findMeshCache(const std::string& filepath) {
    const uint64_t sourceHash = hashFileContents(filepath);
    if (sourceHash == 0) {
        return std::nullopt;
    }

    const std::string stem = std::filesystem::path(filepath).stem().string();
    const std::filesystem::path cachePath =
        cacheRoot() / "meshes" / (stem + "_" + toHexString(sourceHash) + ".mesh.bin");

    std::ifstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::info("AssetCache: mesh cache miss for '{}'", stem);
        return std::nullopt;
    }

    MeshCacheHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || file.gcount() != sizeof(header)) {
        spdlog::warn("AssetCache: corrupt mesh cache header for '{}'", stem);
        return std::nullopt;
    }

    if (std::memcmp(header.magic, "MSH", 3) != 0 || header.magic[3] != '\0') {
        spdlog::warn("AssetCache: invalid mesh cache magic for '{}'", stem);
        return std::nullopt;
    }

    if (header.version != 1) {
        spdlog::info("AssetCache: mesh cache version mismatch for '{}' (got {})", stem, header.version);
        return std::nullopt;
    }

    if (header.contentHash != sourceHash) {
        spdlog::info("AssetCache: mesh cache stale for '{}' (hash mismatch)", stem);
        return std::nullopt;
    }

    const size_t vertexFloats = static_cast<size_t>(header.vertexCount) * 11;
    const size_t indexCount = static_cast<size_t>(header.indexCount);

    CachedMeshData result;
    result.interleavedVertices.resize(vertexFloats);
    result.indices.resize(indexCount);
    result.aabbMin = glm::vec3(header.aabbMin[0], header.aabbMin[1], header.aabbMin[2]);
    result.aabbMax = glm::vec3(header.aabbMax[0], header.aabbMax[1], header.aabbMax[2]);

    file.read(reinterpret_cast<char*>(result.interleavedVertices.data()),
              static_cast<std::streamsize>(vertexFloats * sizeof(float)));
    file.read(reinterpret_cast<char*>(result.indices.data()),
              static_cast<std::streamsize>(indexCount * sizeof(uint32_t)));

    if (!file) {
        spdlog::warn("AssetCache: truncated mesh cache data for '{}'", stem);
        return std::nullopt;
    }

    spdlog::info("AssetCache: mesh cache hit for '{}'", stem);
    return result;
}

std::optional<CachedMeshData> AssetCache::findMeshCache(const std::string& sourceFilePath,
                                                         const std::string& cacheLabel) {
    const uint64_t sourceHash = hashFileContents(sourceFilePath);
    if (sourceHash == 0) {
        return std::nullopt;
    }

    const std::filesystem::path cachePath =
        cacheRoot() / "meshes" / (cacheLabel + "_" + toHexString(sourceHash) + ".mesh.bin");

    std::ifstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::info("AssetCache: mesh cache miss for '{}'", cacheLabel);
        return std::nullopt;
    }

    MeshCacheHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || file.gcount() != sizeof(header)) {
        spdlog::warn("AssetCache: corrupt mesh cache header for '{}'", cacheLabel);
        return std::nullopt;
    }

    if (std::memcmp(header.magic, "MSH", 3) != 0 || header.magic[3] != '\0') {
        spdlog::warn("AssetCache: invalid mesh cache magic for '{}'", cacheLabel);
        return std::nullopt;
    }

    if (header.version != 1) {
        spdlog::info("AssetCache: mesh cache version mismatch for '{}' (got {})", cacheLabel, header.version);
        return std::nullopt;
    }

    if (header.contentHash != sourceHash) {
        spdlog::info("AssetCache: mesh cache stale for '{}' (hash mismatch)", cacheLabel);
        return std::nullopt;
    }

    const size_t vertexFloats = static_cast<size_t>(header.vertexCount) * 11;
    const size_t indexCount = static_cast<size_t>(header.indexCount);

    CachedMeshData result;
    result.interleavedVertices.resize(vertexFloats);
    result.indices.resize(indexCount);
    result.aabbMin = glm::vec3(header.aabbMin[0], header.aabbMin[1], header.aabbMin[2]);
    result.aabbMax = glm::vec3(header.aabbMax[0], header.aabbMax[1], header.aabbMax[2]);

    file.read(reinterpret_cast<char*>(result.interleavedVertices.data()),
              static_cast<std::streamsize>(vertexFloats * sizeof(float)));
    file.read(reinterpret_cast<char*>(result.indices.data()),
              static_cast<std::streamsize>(indexCount * sizeof(uint32_t)));

    if (!file) {
        spdlog::warn("AssetCache: truncated mesh cache data for '{}'", cacheLabel);
        return std::nullopt;
    }

    spdlog::info("AssetCache: mesh cache hit for '{}'", cacheLabel);
    return result;
}

void AssetCache::writeMeshCache(const std::string& filepath,
                                const std::vector<float>& interleavedVertices,
                                const std::vector<uint32_t>& indices,
                                const glm::vec3& aabbMin,
                                const glm::vec3& aabbMax) {
    const uint64_t sourceHash = hashFileContents(filepath);
    if (sourceHash == 0) {
        return;
    }

    const std::string stem = std::filesystem::path(filepath).stem().string();
    const std::filesystem::path cacheDir = cacheRoot() / "meshes";
    std::filesystem::create_directories(cacheDir);

    const std::filesystem::path cachePath =
        cacheDir / (stem + "_" + toHexString(sourceHash) + ".mesh.bin");

    MeshCacheHeader header{};
    header.magic[0] = 'M';
    header.magic[1] = 'S';
    header.magic[2] = 'H';
    header.magic[3] = '\0';
    header.version = 1;
    std::memset(header.padding, 0, sizeof(header.padding));
    header.contentHash = sourceHash;
    header.vertexCount = static_cast<uint32_t>(interleavedVertices.size() / 11);
    header.indexCount = static_cast<uint32_t>(indices.size());
    header.aabbMin[0] = aabbMin.x;
    header.aabbMin[1] = aabbMin.y;
    header.aabbMin[2] = aabbMin.z;
    header.aabbMax[0] = aabbMax.x;
    header.aabbMax[1] = aabbMax.y;
    header.aabbMax[2] = aabbMax.z;

    std::ofstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("AssetCache: cannot write mesh cache for '{}'", stem);
        return;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(interleavedVertices.data()),
               static_cast<std::streamsize>(interleavedVertices.size() * sizeof(float)));
    file.write(reinterpret_cast<const char*>(indices.data()),
               static_cast<std::streamsize>(indices.size() * sizeof(uint32_t)));

    spdlog::info("AssetCache: wrote mesh cache for '{}' ({} verts, {} indices)",
                 stem, header.vertexCount, header.indexCount);
}

void AssetCache::writeMeshCache(const std::string& sourceFilePath,
                                const std::string& cacheLabel,
                                const std::vector<float>& interleavedVertices,
                                const std::vector<uint32_t>& indices,
                                const glm::vec3& aabbMin,
                                const glm::vec3& aabbMax) {
    const uint64_t sourceHash = hashFileContents(sourceFilePath);
    if (sourceHash == 0) {
        return;
    }

    const std::filesystem::path cacheDir = cacheRoot() / "meshes";
    std::filesystem::create_directories(cacheDir);

    const std::filesystem::path cachePath =
        cacheDir / (cacheLabel + "_" + toHexString(sourceHash) + ".mesh.bin");

    MeshCacheHeader header{};
    header.magic[0] = 'M';
    header.magic[1] = 'S';
    header.magic[2] = 'H';
    header.magic[3] = '\0';
    header.version = 1;
    std::memset(header.padding, 0, sizeof(header.padding));
    header.contentHash = sourceHash;
    header.vertexCount = static_cast<uint32_t>(interleavedVertices.size() / 11);
    header.indexCount = static_cast<uint32_t>(indices.size());
    header.aabbMin[0] = aabbMin.x;
    header.aabbMin[1] = aabbMin.y;
    header.aabbMin[2] = aabbMin.z;
    header.aabbMax[0] = aabbMax.x;
    header.aabbMax[1] = aabbMax.y;
    header.aabbMax[2] = aabbMax.z;

    std::ofstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("AssetCache: cannot write mesh cache for '{}'", cacheLabel);
        return;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(interleavedVertices.data()),
               static_cast<std::streamsize>(interleavedVertices.size() * sizeof(float)));
    file.write(reinterpret_cast<const char*>(indices.data()),
               static_cast<std::streamsize>(indices.size() * sizeof(uint32_t)));

    spdlog::info("AssetCache: wrote mesh cache for '{}' ({} verts, {} indices)",
                 cacheLabel, header.vertexCount, header.indexCount);
}

std::optional<CachedTextureData> AssetCache::findTextureCache(const std::string& name,
                                                               uint64_t paramHash) {
    const std::string safeName = sanitizeCacheName(name);
    const std::string hexHash = toHexString(paramHash);
    const std::filesystem::path cachePath =
        cacheRoot() / "textures" / (safeName + "_" + hexHash + ".tex.bin");

    std::ifstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::info("AssetCache: texture cache miss for '{}'", name);
        return std::nullopt;
    }

    TextureCacheHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || file.gcount() != sizeof(header)) {
        spdlog::warn("AssetCache: corrupt texture cache header for '{}'", name);
        return std::nullopt;
    }

    if (std::memcmp(header.magic, "TEX", 3) != 0 || header.magic[3] != '\0') {
        spdlog::warn("AssetCache: invalid texture cache magic for '{}'", name);
        return std::nullopt;
    }

    if (header.version != 1) {
        spdlog::info("AssetCache: texture cache version mismatch for '{}'", name);
        return std::nullopt;
    }

    if (header.paramHash != paramHash) {
        spdlog::info("AssetCache: texture cache param hash mismatch for '{}'", name);
        return std::nullopt;
    }

    const size_t pixelCount =
        static_cast<size_t>(header.width) * static_cast<size_t>(header.height) *
        static_cast<size_t>(header.channels);

    CachedTextureData result;
    result.width = header.width;
    result.height = header.height;
    result.channels = header.channels;
    result.pixels.resize(pixelCount);

    file.read(reinterpret_cast<char*>(result.pixels.data()),
              static_cast<std::streamsize>(pixelCount));

    if (!file) {
        spdlog::warn("AssetCache: truncated texture cache data for '{}'", name);
        return std::nullopt;
    }

    spdlog::info("AssetCache: texture cache hit for '{}'", name);
    return result;
}

void AssetCache::writeTextureCache(const std::string& name,
                                   uint64_t paramHash,
                                   const std::vector<uint8_t>& pixels,
                                   uint16_t width,
                                   uint16_t height,
                                   uint8_t channels) {
    const std::string safeName = sanitizeCacheName(name);
    const std::string hexHash = toHexString(paramHash);
    const std::filesystem::path cacheDir = cacheRoot() / "textures";
    std::filesystem::create_directories(cacheDir);

    const std::filesystem::path cachePath =
        cacheDir / (safeName + "_" + hexHash + ".tex.bin");

    TextureCacheHeader header{};
    header.magic[0] = 'T';
    header.magic[1] = 'E';
    header.magic[2] = 'X';
    header.magic[3] = '\0';
    header.version = 1;
    header.padding1 = 0;
    header.width = width;
    header.height = height;
    header.channels = channels;
    header.padding2 = 0;
    header.paramHash = paramHash;
    header.reserved = 0;

    std::ofstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("AssetCache: cannot write texture cache for '{}'", name);
        return;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));

    spdlog::info("AssetCache: wrote texture cache for '{}' ({}x{}, {} ch)",
                 name, width, height, channels);
}
