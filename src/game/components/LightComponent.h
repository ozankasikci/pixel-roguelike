#pragma once
#include <glm/glm.hpp>

struct LightComponent {
    glm::vec3 color{1.0f};
    float radius = 10.0f;
    float intensity = 1.0f;
};
