#pragma once

#include <glm/glm.hpp>

class Mesh;

#include <string>

struct MeshComponent {
    std::string meshId;
    Mesh* mesh = nullptr;     // non-owning pointer, mesh lifetime managed by scene/resource manager
    glm::vec3 tint{1.0f};
    std::string materialId;
};
