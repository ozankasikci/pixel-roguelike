#pragma once
#include "game/rendering/MaterialKind.h"

#include <glm/glm.hpp>

class Mesh;

#include <string>

struct MeshComponent {
    std::string meshId;
    Mesh* mesh = nullptr;     // non-owning pointer, mesh lifetime managed by scene/resource manager
    glm::mat4 modelOverride;  // optional: pre-computed model matrix (for non-standard transforms like arch segments)
    bool useModelOverride = false;
    glm::vec3 tint{1.0f};
    MaterialKind material = MaterialKind::Stone;
    std::string materialId;
};
