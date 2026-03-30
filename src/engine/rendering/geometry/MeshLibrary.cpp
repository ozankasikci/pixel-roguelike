#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/AssetCache.h"
#include "engine/rendering/assets/ModelLoader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshGeometry.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

void MeshLibrary::registerMesh(const std::string& name, std::unique_ptr<Mesh> mesh) {
    meshes_[name] = std::move(mesh);
}

void MeshLibrary::registerFileAlias(const std::string& name, const std::string& filepath) {
    fileAliases_[name] = filepath;
}

Mesh* MeshLibrary::get(const std::string& name) {
    auto it = meshes_.find(name);
    if (it != meshes_.end()) {
        return it->second.get();
    }

    auto alias = fileAliases_.find(name);
    if (alias != fileAliases_.end()) {
        loadFromFile(name, alias->second);
        it = meshes_.find(name);
        if (it != meshes_.end()) {
            return it->second.get();
        }
    }

    return nullptr;
}

Mesh* MeshLibrary::get(const std::string& name) const {
    auto it = meshes_.find(name);
    if (it == meshes_.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool MeshLibrary::has(const std::string& name) const {
    return meshes_.find(name) != meshes_.end() || fileAliases_.find(name) != fileAliases_.end();
}

std::vector<std::string> MeshLibrary::names() const {
    std::vector<std::string> result;
    result.reserve(meshes_.size() + fileAliases_.size());
    for (const auto& [name, mesh] : meshes_) {
        (void)mesh;
        result.push_back(name);
    }
    for (const auto& [name, path] : fileAliases_) {
        (void)path;
        if (meshes_.find(name) == meshes_.end()) {
            result.push_back(name);
        }
    }
    return result;
}

void MeshLibrary::registerDefaults() {
    registerMesh("cube", std::make_unique<Mesh>(Mesh::createCube(1.0f)));
    registerMesh("plane", std::make_unique<Mesh>(Mesh::createPlane(1.0f)));
    constexpr int defaultCylinderSegments = 24;
    registerMesh("cylinder", std::make_unique<Mesh>(Mesh::createCylinder(1.0f, 1.0f, defaultCylinderSegments)));
    registerMesh("cylinder_wide", std::make_unique<Mesh>(Mesh::createCylinder(1.5f, 1.0f, defaultCylinderSegments)));
    registerMesh("cylinder_cap", std::make_unique<Mesh>(Mesh::createCylinder(1.4f, 1.0f, defaultCylinderSegments)));
    spdlog::info("MeshLibrary: registered {} default meshes", meshes_.size());
}

void MeshLibrary::loadFromFile(const std::string& name, const std::string& filepath) {
    std::string resolvedPath = resolveProjectPath(filepath);

    // Try disk cache first
    auto cached = AssetCache::findMeshCache(resolvedPath);
    if (cached) {
        registerMesh(name, std::make_unique<Mesh>(
            cached->interleavedVertices, cached->indices,
            cached->aabbMin, cached->aabbMax));
        return;
    }

    // Cache miss: full load path
    RawMeshData raw = ModelLoader::loadRaw(resolvedPath);
    if (raw.positions.empty() || raw.indices.empty()) {
        spdlog::error("MeshLibrary: failed to load mesh '{}' from '{}'", name, filepath);
        return;
    }

    // Create mesh (this interleaves the data and computes AABB)
    auto mesh = std::make_unique<Mesh>(
        raw.positions, raw.normals, raw.uvs, raw.tangents, raw.indices);

    // Build interleaved data for cache storage
    std::vector<float> interleaved;
    interleaved.reserve(raw.positions.size() * 11);
    for (size_t i = 0; i < raw.positions.size(); ++i) {
        interleaved.push_back(raw.positions[i].x);
        interleaved.push_back(raw.positions[i].y);
        interleaved.push_back(raw.positions[i].z);
        const auto& n = i < raw.normals.size() ? raw.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
        interleaved.push_back(n.x);
        interleaved.push_back(n.y);
        interleaved.push_back(n.z);
        const auto& uv = i < raw.uvs.size() ? raw.uvs[i] : glm::vec2(0.0f, 0.0f);
        interleaved.push_back(uv.x);
        interleaved.push_back(uv.y);
        const auto& t = i < raw.tangents.size() ? raw.tangents[i] : glm::vec3(1.0f, 0.0f, 0.0f);
        interleaved.push_back(t.x);
        interleaved.push_back(t.y);
        interleaved.push_back(t.z);
    }
    AssetCache::writeMeshCache(resolvedPath, interleaved, raw.indices,
                               mesh->aabbMin(), mesh->aabbMax());

    registerMesh(name, std::move(mesh));
}

void MeshLibrary::loadFromFileMulti(const std::string& baseName, const std::string& filepath) {
    std::string resolvedPath = resolveProjectPath(filepath);

    std::vector<NamedRawMeshData> groups = ModelLoader::loadRawMulti(resolvedPath);
    if (groups.empty()) {
        spdlog::error("MeshLibrary: loadFromFileMulti failed for '{}' (no material groups)", filepath);
        return;
    }

    for (auto& entry : groups) {
        const std::string meshName = baseName + "#" + entry.name;
        const std::string cacheKey = resolvedPath + "#" + entry.name;

        // Try disk cache first
        auto cached = AssetCache::findMeshCache(cacheKey);
        if (cached) {
            registerMesh(meshName, std::make_unique<Mesh>(
                cached->interleavedVertices, cached->indices,
                cached->aabbMin, cached->aabbMax));
            continue;
        }

        // Cache miss: build mesh from raw data
        RawMeshData& raw = entry.mesh;
        if (raw.positions.empty() || raw.indices.empty()) {
            spdlog::warn("MeshLibrary: skipping empty material group '{}' in '{}'", entry.name, filepath);
            continue;
        }

        auto mesh = std::make_unique<Mesh>(
            raw.positions, raw.normals, raw.uvs, raw.tangents, raw.indices);

        // Build interleaved data for cache storage
        std::vector<float> interleaved;
        interleaved.reserve(raw.positions.size() * 11);
        for (size_t i = 0; i < raw.positions.size(); ++i) {
            interleaved.push_back(raw.positions[i].x);
            interleaved.push_back(raw.positions[i].y);
            interleaved.push_back(raw.positions[i].z);
            const auto& n = i < raw.normals.size() ? raw.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            interleaved.push_back(n.x);
            interleaved.push_back(n.y);
            interleaved.push_back(n.z);
            const auto& uv = i < raw.uvs.size() ? raw.uvs[i] : glm::vec2(0.0f, 0.0f);
            interleaved.push_back(uv.x);
            interleaved.push_back(uv.y);
            const auto& t = i < raw.tangents.size() ? raw.tangents[i] : glm::vec3(1.0f, 0.0f, 0.0f);
            interleaved.push_back(t.x);
            interleaved.push_back(t.y);
            interleaved.push_back(t.z);
        }
        AssetCache::writeMeshCache(cacheKey, interleaved, raw.indices,
                                   mesh->aabbMin(), mesh->aabbMax());

        registerMesh(meshName, std::move(mesh));
    }

    spdlog::info("MeshLibrary: registered {} submeshes from '{}' as '{}#...'",
                 groups.size(), filepath, baseName);
}

void MeshLibrary::clear() {
    meshes_.clear();
    fileAliases_.clear();
}
