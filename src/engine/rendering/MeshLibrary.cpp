#include "engine/rendering/MeshLibrary.h"
#include "engine/rendering/Mesh.h"

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
    registerMesh("cylinder", std::make_unique<Mesh>(Mesh::createCylinder(1.0f, 1.0f, 12)));
    registerMesh("cylinder_wide", std::make_unique<Mesh>(Mesh::createCylinder(1.5f, 1.0f, 12)));
    registerMesh("cylinder_cap", std::make_unique<Mesh>(Mesh::createCylinder(1.4f, 1.0f, 12)));
    spdlog::info("MeshLibrary: registered {} default meshes", meshes_.size());
}

void MeshLibrary::clear() {
    meshes_.clear();
}
