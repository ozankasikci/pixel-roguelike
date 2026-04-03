#pragma once

#include <glm/glm.hpp>

struct ReflectionProbeComponent {
    glm::vec3 extents{4.0f, 3.0f, 4.0f};
    float blendDistance = 1.0f;
    float intensity = 1.0f;
    bool boxProjection = true;
    bool dirty = true;
};
