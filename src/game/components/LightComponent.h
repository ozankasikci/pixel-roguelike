#pragma once

#include "engine/rendering/lighting/RenderLight.h"

#include <glm/glm.hpp>

struct LightComponent {
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float radius = 10.0f;
    float intensity = 1.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 30.0f;
    // Area light fields (AreaRect and Tube):
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float width = 1.0f;
    float height = 0.5f;
    bool doubleSided = false;
    bool castsShadows = false;
};
