#pragma once
#include <glm/glm.hpp>

class Mesh;

struct MeshComponent {
    Mesh* mesh = nullptr;     // non-owning pointer, mesh lifetime managed by scene/resource manager
    glm::mat4 modelOverride;  // optional: pre-computed model matrix (for non-standard transforms like arch segments)
    bool useModelOverride = false;
};
