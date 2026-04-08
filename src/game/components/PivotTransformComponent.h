#pragma once

#include <glm/glm.hpp>

struct PivotTransformComponent {
    glm::vec3 pivot{0.0f};          // Local-space hinge offset
    glm::vec3 meshCenter{0.0f};     // AABB center for frame alignment
    glm::vec3 scale{1.0f};          // Mesh scale
    float closedYawDeg = 0.0f;      // Assembly yaw when closed
    float currentYawDeg = 0.0f;     // Current yaw (animated by DoorAnimationSystem)
};
