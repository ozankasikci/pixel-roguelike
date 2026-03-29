#include "engine/rendering/assets/AssimpLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr unsigned kAssimpImportFlags =
    aiProcess_Triangulate
    | aiProcess_JoinIdenticalVertices
    | aiProcess_ImproveCacheLocality
    | aiProcess_RemoveRedundantMaterials
    | aiProcess_FindInvalidData
    | aiProcess_GenUVCoords
    | aiProcess_TransformUVCoords
    | aiProcess_GenSmoothNormals
    | aiProcess_CalcTangentSpace
    | aiProcess_PreTransformVertices
    | aiProcess_SortByPType;

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    const float lengthSq = glm::dot(value, value);
    if (lengthSq <= 1e-12f) {
        return fallback;
    }
    return value / std::sqrt(lengthSq);
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
        normal = safeNormalize(normal, glm::vec3(0.0f, 1.0f, 0.0f));
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

RawMeshData loadMergedRaw(const aiScene& scene) {
    RawMeshData merged;

    for (unsigned meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex) {
        const aiMesh* sourceMesh = scene.mMeshes[meshIndex];
        if (sourceMesh == nullptr || sourceMesh->mNumVertices == 0) {
            continue;
        }

        RawMeshData part;
        part.positions.reserve(sourceMesh->mNumVertices);
        if (sourceMesh->HasNormals()) {
            part.normals.reserve(sourceMesh->mNumVertices);
        }
        if (sourceMesh->HasTextureCoords(0)) {
            part.uvs.reserve(sourceMesh->mNumVertices);
        }
        if (sourceMesh->HasTangentsAndBitangents()) {
            part.tangents.reserve(sourceMesh->mNumVertices);
        }

        for (unsigned vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex) {
            const aiVector3D& position = sourceMesh->mVertices[vertexIndex];
            part.positions.emplace_back(position.x, position.y, position.z);

            if (sourceMesh->HasNormals()) {
                const aiVector3D& normal = sourceMesh->mNormals[vertexIndex];
                part.normals.push_back(safeNormalize(glm::vec3(normal.x, normal.y, normal.z),
                                                     glm::vec3(0.0f, 1.0f, 0.0f)));
            }

            if (sourceMesh->HasTextureCoords(0)) {
                const aiVector3D& uv = sourceMesh->mTextureCoords[0][vertexIndex];
                part.uvs.emplace_back(uv.x, uv.y);
            }

            if (sourceMesh->HasTangentsAndBitangents()) {
                const aiVector3D& tangent = sourceMesh->mTangents[vertexIndex];
                part.tangents.push_back(safeNormalize(glm::vec3(tangent.x, tangent.y, tangent.z),
                                                      glm::vec3(1.0f, 0.0f, 0.0f)));
            }
        }

        part.indices.reserve(sourceMesh->mNumFaces * 3);
        for (unsigned faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex) {
            const aiFace& face = sourceMesh->mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }
            part.indices.push_back(face.mIndices[0]);
            part.indices.push_back(face.mIndices[1]);
            part.indices.push_back(face.mIndices[2]);
        }

        if (part.indices.empty()) {
            part.indices.resize(part.positions.size());
            for (uint32_t index = 0; index < part.indices.size(); ++index) {
                part.indices[index] = index;
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
        merged.positions.insert(merged.positions.end(), part.positions.begin(), part.positions.end());
        merged.normals.insert(merged.normals.end(), part.normals.begin(), part.normals.end());
        merged.uvs.insert(merged.uvs.end(), part.uvs.begin(), part.uvs.end());
        merged.tangents.insert(merged.tangents.end(), part.tangents.begin(), part.tangents.end());
        for (uint32_t index : part.indices) {
            merged.indices.push_back(baseIndex + index);
        }
    }

    return merged;
}

} // namespace

std::unique_ptr<Mesh> AssimpLoader::load(const std::string& filepath) {
    RawMeshData raw = loadRaw(filepath);
    if (raw.positions.empty() || raw.indices.empty()) {
        return nullptr;
    }
    return std::make_unique<Mesh>(raw.positions, raw.normals, raw.uvs, raw.tangents, raw.indices);
}

RawMeshData AssimpLoader::loadRaw(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath, kAssimpImportFlags);
    if (scene == nullptr) {
        spdlog::error("AssimpLoader::loadRaw: failed to load '{}': {}", filepath, importer.GetErrorString());
        return {};
    }

    if (!scene->HasMeshes()) {
        spdlog::error("AssimpLoader::loadRaw: no mesh primitives in '{}'", filepath);
        return {};
    }

    RawMeshData result = loadMergedRaw(*scene);
    if (result.positions.empty() || result.indices.empty()) {
        spdlog::error("AssimpLoader::loadRaw: merged mesh empty for '{}'", filepath);
        return {};
    }

    spdlog::info("AssimpLoader::loadRaw: loaded '{}' ({} verts, {} tris)",
                 filepath,
                 result.positions.size(),
                 result.indices.size() / 3);
    return result;
}
