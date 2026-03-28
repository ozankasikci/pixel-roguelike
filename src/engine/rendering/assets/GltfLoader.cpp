#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "engine/rendering/assets/GltfLoader.h"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <filesystem>
#include <functional>

namespace {

bool ignoreImageData(tinygltf::Image*,
                     const int,
                     std::string*,
                     std::string*,
                     int,
                     int,
                     const unsigned char*,
                     int,
                     void*) {
    return true;
}

bool loadModelFile(const std::string& filepath,
                   tinygltf::TinyGLTF& loader,
                   tinygltf::Model& model,
                   std::string& err,
                   std::string& warn) {
    loader.SetImageLoader(ignoreImageData, nullptr);
    const auto extension = std::filesystem::path(filepath).extension().string();
    if (extension == ".gltf") {
        return loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }
    return loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
}

const uint8_t* getAccessorData(const tinygltf::Model& model, int accessorIdx) {
    const auto& acc = model.accessors[accessorIdx];
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

size_t accessorStride(const tinygltf::Model& model, int accessorIdx) {
    const auto& acc = model.accessors[accessorIdx];
    const auto& bv = model.bufferViews[acc.bufferView];
    return bv.byteStride != 0 ? static_cast<size_t>(bv.byteStride) : tinygltf::GetComponentSizeInBytes(acc.componentType) * tinygltf::GetNumComponentsInType(acc.type);
}

std::vector<glm::vec2> loadVec2Accessor(const tinygltf::Model& model, int accessorIdx) {
    const auto& acc = model.accessors[accessorIdx];
    const uint8_t* raw = getAccessorData(model, accessorIdx);
    const size_t stride = accessorStride(model, accessorIdx);
    std::vector<glm::vec2> values(acc.count, glm::vec2(0.0f));
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(raw + i * stride);
        values[i] = glm::vec2(src[0], src[1]);
    }
    return values;
}

std::vector<glm::vec3> loadVec3Accessor(const tinygltf::Model& model, int accessorIdx) {
    const auto& acc = model.accessors[accessorIdx];
    const uint8_t* raw = getAccessorData(model, accessorIdx);
    const size_t stride = accessorStride(model, accessorIdx);
    std::vector<glm::vec3> values(acc.count, glm::vec3(0.0f));
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(raw + i * stride);
        values[i] = glm::vec3(src[0], src[1], src[2]);
    }
    return values;
}

std::vector<glm::vec3> loadTangentAccessor(const tinygltf::Model& model, int accessorIdx) {
    const auto& acc = model.accessors[accessorIdx];
    const uint8_t* raw = getAccessorData(model, accessorIdx);
    const size_t stride = accessorStride(model, accessorIdx);
    std::vector<glm::vec3> values(acc.count, glm::vec3(1.0f, 0.0f, 0.0f));
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(raw + i * stride);
        values[i] = glm::vec3(src[0], src[1], src[2]);
    }
    return values;
}

std::vector<uint32_t> loadIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    if (primitive.indices < 0) {
        return {};
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
    return indices;
}

void generateSmoothNormals(RawMeshData& mesh) {
    mesh.normals.assign(mesh.positions.size(), glm::vec3(0.0f));
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t ia = mesh.indices[i + 0];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];
        const glm::vec3 a = mesh.positions[ia];
        const glm::vec3 b = mesh.positions[ib];
        const glm::vec3 c = mesh.positions[ic];
        const glm::vec3 faceNormal = glm::cross(b - a, c - a);
        if (glm::dot(faceNormal, faceNormal) <= 1e-12f) {
            continue;
        }
        mesh.normals[ia] += faceNormal;
        mesh.normals[ib] += faceNormal;
        mesh.normals[ic] += faceNormal;
    }

    for (auto& normal : mesh.normals) {
        if (glm::dot(normal, normal) <= 1e-12f) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            normal = glm::normalize(normal);
        }
    }
}

void generateProjectedUVs(RawMeshData& mesh) {
    mesh.uvs.resize(mesh.positions.size(), glm::vec2(0.0f));
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const glm::vec3 normal = i < mesh.normals.size() ? glm::abs(mesh.normals[i]) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 position = mesh.positions[i];
        if (normal.y >= normal.x && normal.y >= normal.z) {
            mesh.uvs[i] = glm::vec2(position.x, position.z);
        } else if (normal.x >= normal.z) {
            mesh.uvs[i] = glm::vec2(position.z, position.y);
        } else {
            mesh.uvs[i] = glm::vec2(position.x, position.y);
        }
    }
}

glm::mat4 nodeLocalTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        return glm::make_mat4(node.matrix.data());
    }

    glm::mat4 transform(1.0f);
    if (node.translation.size() == 3) {
        transform = glm::translate(transform, glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        ));
    }
    if (node.rotation.size() == 4) {
        glm::quat rotation(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );
        transform *= glm::mat4_cast(rotation);
    }
    if (node.scale.size() == 3) {
        transform = glm::scale(transform, glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])
        ));
    }
    return transform;
}

RawMeshData loadMergedRaw(const tinygltf::Model& model) {
    RawMeshData merged;

    std::function<void(int, const glm::mat4&)> visitNode = [&](int nodeIndex, const glm::mat4& parentTransform) {
        const auto& node = model.nodes[nodeIndex];
        const glm::mat4 transform = parentTransform * nodeLocalTransform(node);

        if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
            const auto& mesh = model.meshes[node.mesh];
            for (const auto& primitive : mesh.primitives) {
                auto posIt = primitive.attributes.find("POSITION");
                if (posIt == primitive.attributes.end()) {
                    continue;
                }

                RawMeshData part;
                part.positions = loadVec3Accessor(model, posIt->second);

                auto normIt = primitive.attributes.find("NORMAL");
                if (normIt != primitive.attributes.end()) {
                    part.normals = loadVec3Accessor(model, normIt->second);
                }

                auto uvIt = primitive.attributes.find("TEXCOORD_0");
                if (uvIt != primitive.attributes.end()) {
                    part.uvs = loadVec2Accessor(model, uvIt->second);
                }

                auto tangentIt = primitive.attributes.find("TANGENT");
                if (tangentIt != primitive.attributes.end()) {
                    part.tangents = loadTangentAccessor(model, tangentIt->second);
                }

                part.indices = loadIndices(model, primitive);
                if (part.indices.empty()) {
                    part.indices.resize(part.positions.size());
                    for (uint32_t i = 0; i < part.indices.size(); ++i) {
                        part.indices[i] = i;
                    }
                }
                if (part.normals.size() != part.positions.size()) {
                    generateSmoothNormals(part);
                }
                if (part.uvs.size() != part.positions.size()) {
                    generateProjectedUVs(part);
                }
                if (part.tangents.size() != part.positions.size()) {
                    generateTangents(part);
                }

                const uint32_t baseIndex = static_cast<uint32_t>(merged.positions.size());
                const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
                for (size_t i = 0; i < part.positions.size(); ++i) {
                    merged.positions.push_back(glm::vec3(transform * glm::vec4(part.positions[i], 1.0f)));
                    merged.normals.push_back(glm::normalize(normalMatrix * part.normals[i]));
                    merged.uvs.push_back(part.uvs[i]);
                    merged.tangents.push_back(glm::normalize(normalMatrix * part.tangents[i]));
                }
                for (uint32_t index : part.indices) {
                    merged.indices.push_back(baseIndex + index);
                }
            }
        }

        for (int child : node.children) {
            visitNode(child, transform);
        }
    };

    if (model.scenes.empty()) {
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            visitNode(static_cast<int>(i), glm::mat4(1.0f));
        }
    } else {
        const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        for (int root : model.scenes[sceneIndex].nodes) {
            visitNode(root, glm::mat4(1.0f));
        }
    }

    return merged;
}

} // namespace

std::unique_ptr<Mesh> GltfLoader::load(const std::string& filepath) {
    RawMeshData raw = loadRaw(filepath);
    if (raw.positions.empty() || raw.indices.empty()) {
        return nullptr;
    }
    return std::make_unique<Mesh>(raw.positions, raw.normals, raw.uvs, raw.tangents, raw.indices);
}

RawMeshData GltfLoader::loadRaw(const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = loadModelFile(filepath, loader, model, err, warn);
    if (!warn.empty()) spdlog::warn("GltfLoader: {}", warn);
    if (!ok) {
        spdlog::error("GltfLoader::loadRaw: failed to load '{}': {}", filepath, err);
        return {};
    }

    if (model.meshes.empty()) {
        spdlog::error("GltfLoader::loadRaw: no mesh primitives in '{}'", filepath);
        return {};
    }

    RawMeshData result = loadMergedRaw(model);
    if (result.positions.empty() || result.indices.empty()) {
        spdlog::error("GltfLoader::loadRaw: merged mesh empty for '{}'", filepath);
        return {};
    }

    spdlog::info("GltfLoader::loadRaw: loaded '{}' ({} verts, {} tris)",
                 filepath, result.positions.size(), result.indices.size() / 3);

    return result;
}
