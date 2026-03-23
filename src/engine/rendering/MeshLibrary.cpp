#include "engine/rendering/MeshLibrary.h"
#include "engine/rendering/Mesh.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>

// Cylinder helper (moved from CathedralScene.cpp)
static Mesh createCylinder(float radius, float height, int segments) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;

    for (int ring = 0; ring <= 1; ++ring) {
        float y = ring * height;
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * 2.0f * 3.14159265f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            positions.push_back(glm::vec3(x, y, z));
            normals.push_back(glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle))));
        }
    }

    int stride = segments + 1;
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = i, br = i + 1, tl = i + stride, tr = i + 1 + stride;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(br); indices.push_back(tr); indices.push_back(tl);
    }

    uint32_t botCenter = (uint32_t)positions.size();
    positions.push_back(glm::vec3(0, 0, 0));
    normals.push_back(glm::vec3(0, -1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(botCenter);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    uint32_t topCenter = (uint32_t)positions.size();
    positions.push_back(glm::vec3(0, height, 0));
    normals.push_back(glm::vec3(0, 1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(i + stride);
        indices.push_back(i + 1 + stride);
    }

    return Mesh(positions, normals, indices);
}

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
    registerMesh("cylinder", std::make_unique<Mesh>(createCylinder(1.0f, 1.0f, 12)));
    registerMesh("cylinder_wide", std::make_unique<Mesh>(createCylinder(1.5f, 1.0f, 12)));
    registerMesh("cylinder_cap", std::make_unique<Mesh>(createCylinder(1.4f, 1.0f, 12)));
    spdlog::info("MeshLibrary: registered {} default meshes", meshes_.size());
}

void MeshLibrary::clear() {
    meshes_.clear();
}
