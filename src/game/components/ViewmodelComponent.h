#pragma once
#include <glm/glm.hpp>

struct ViewmodelComponent {
    glm::vec3 viewOffset{0.090f, -0.255f, -0.395f};    // position in view/camera space
    glm::vec3 rotation{-40.5f, -53.5f, 215.5f};        // pitch, yaw, roll in degrees
    glm::vec3 scale{0.003f};                           // scale for the mesh in view space
    glm::vec3 meshCenter{-136.5f, -363.5f, -0.6f};      // mesh center (subtracted to center at origin)

    // Bob animation
    float bobSpeed = 0.7f;       // oscillations per second
    float bobAmplitude = 0.002f; // vertical displacement in view space
    float bobTime = 0.0f;        // accumulated time (updated by render system)
};
