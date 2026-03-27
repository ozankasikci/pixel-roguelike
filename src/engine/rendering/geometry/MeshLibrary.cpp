#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/GltfLoader.h"
#include "engine/rendering/geometry/Mesh.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

void MeshLibrary::registerMesh(const std::string& name, std::unique_ptr<Mesh> mesh) {
    meshes_[name] = std::move(mesh);
}

Mesh* MeshLibrary::get(const std::string& name) const {
    auto it = meshes_.find(name);
    if (it == meshes_.end()) return nullptr;
    return it->second.get();
}

bool MeshLibrary::has(const std::string& name) const {
    return meshes_.find(name) != meshes_.end();
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
    auto mesh = GltfLoader::load(resolveProjectPath(filepath));
    if (mesh) {
        registerMesh(name, std::move(mesh));
    } else {
        spdlog::error("MeshLibrary: failed to load mesh '{}' from '{}'", name, filepath);
    }
}

void MeshLibrary::clear() {
    meshes_.clear();
}
