#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "engine/rendering/assets/GltfLoader.h"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cstring>

std::unique_ptr<Mesh> GltfLoader::load(const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    if (!warn.empty()) spdlog::warn("GltfLoader: {}", warn);
    if (!ok) {
        spdlog::error("GltfLoader: failed to load '{}': {}", filepath, err);
        return nullptr;
    }

    if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
        spdlog::error("GltfLoader: no mesh primitives in '{}'", filepath);
        return nullptr;
    }

    const auto& primitive = model.meshes[0].primitives[0];

    // Helper: extract float data from an accessor
    auto getFloatData = [&](int accessorIdx) -> const float* {
        const auto& acc = model.accessors[accessorIdx];
        const auto& bv = model.bufferViews[acc.bufferView];
        const auto& buf = model.buffers[bv.buffer];
        return reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset);
    };

    // POSITION
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        spdlog::error("GltfLoader: no POSITION attribute in '{}'", filepath);
        return nullptr;
    }
    const auto& posAcc = model.accessors[posIt->second];
    const float* posData = getFloatData(posIt->second);

    std::vector<glm::vec3> positions(posAcc.count);
    std::memcpy(positions.data(), posData, posAcc.count * sizeof(glm::vec3));

    // NORMAL
    std::vector<glm::vec3> normals;
    auto normIt = primitive.attributes.find("NORMAL");
    if (normIt != primitive.attributes.end()) {
        const auto& normAcc = model.accessors[normIt->second];
        const float* normData = getFloatData(normIt->second);
        normals.resize(normAcc.count);
        std::memcpy(normals.data(), normData, normAcc.count * sizeof(glm::vec3));
    } else {
        // Default up-facing normals if missing
        normals.resize(posAcc.count, glm::vec3(0, 1, 0));
    }

    // INDICES
    if (primitive.indices < 0) {
        spdlog::error("GltfLoader: no indices in '{}'", filepath);
        return nullptr;
    }
    const auto& idxAcc = model.accessors[primitive.indices];
    const auto& idxBv = model.bufferViews[idxAcc.bufferView];
    const auto& idxBuf = model.buffers[idxBv.buffer];
    const uint8_t* idxRaw = idxBuf.data.data() + idxBv.byteOffset + idxAcc.byteOffset;

    std::vector<uint32_t> indices(idxAcc.count);
    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::memcpy(indices.data(), idxRaw, idxAcc.count * sizeof(uint32_t));
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(idxRaw);
        for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = src[i];
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = idxRaw[i];
    }

    spdlog::info("GltfLoader: loaded '{}' ({} verts, {} tris)",
                 filepath, positions.size(), indices.size() / 3);

    return std::make_unique<Mesh>(positions, normals, indices);
}

RawMeshData GltfLoader::loadRaw(const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    if (!warn.empty()) spdlog::warn("GltfLoader: {}", warn);
    if (!ok) {
        spdlog::error("GltfLoader::loadRaw: failed to load '{}': {}", filepath, err);
        return {};
    }

    if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
        spdlog::error("GltfLoader::loadRaw: no mesh primitives in '{}'", filepath);
        return {};
    }

    const auto& primitive = model.meshes[0].primitives[0];

    auto getFloatData = [&](int accessorIdx) -> const float* {
        const auto& acc = model.accessors[accessorIdx];
        const auto& bv = model.bufferViews[acc.bufferView];
        const auto& buf = model.buffers[bv.buffer];
        return reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset);
    };

    // POSITION
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        spdlog::error("GltfLoader::loadRaw: no POSITION attribute in '{}'", filepath);
        return {};
    }
    const auto& posAcc = model.accessors[posIt->second];
    const float* posData = getFloatData(posIt->second);

    RawMeshData result;
    result.positions.resize(posAcc.count);
    std::memcpy(result.positions.data(), posData, posAcc.count * sizeof(glm::vec3));

    // NORMAL
    auto normIt = primitive.attributes.find("NORMAL");
    if (normIt != primitive.attributes.end()) {
        const auto& normAcc = model.accessors[normIt->second];
        const float* normData = getFloatData(normIt->second);
        result.normals.resize(normAcc.count);
        std::memcpy(result.normals.data(), normData, normAcc.count * sizeof(glm::vec3));
    } else {
        result.normals.resize(posAcc.count, glm::vec3(0, 1, 0));
    }

    // INDICES
    if (primitive.indices < 0) {
        spdlog::error("GltfLoader::loadRaw: no indices in '{}'", filepath);
        return {};
    }
    const auto& idxAcc = model.accessors[primitive.indices];
    const auto& idxBv = model.bufferViews[idxAcc.bufferView];
    const auto& idxBuf = model.buffers[idxBv.buffer];
    const uint8_t* idxRaw = idxBuf.data.data() + idxBv.byteOffset + idxAcc.byteOffset;

    result.indices.resize(idxAcc.count);
    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::memcpy(result.indices.data(), idxRaw, idxAcc.count * sizeof(uint32_t));
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(idxRaw);
        for (size_t i = 0; i < idxAcc.count; ++i) result.indices[i] = src[i];
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        for (size_t i = 0; i < idxAcc.count; ++i) result.indices[i] = idxRaw[i];
    }

    spdlog::info("GltfLoader::loadRaw: loaded '{}' ({} verts, {} tris)",
                 filepath, result.positions.size(), result.indices.size() / 3);

    return result;
}
