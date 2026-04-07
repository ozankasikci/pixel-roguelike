#pragma once

#include <glm/glm.hpp>

struct DoorLeafComponent {
    glm::vec3 basePosition{0.0f};   // world position of the door group
    glm::vec3 pivot{0.0f};          // mesh-local hinge offset
    glm::vec3 meshCenter{0.0f};     // AABB center of the door mesh (for frame alignment)
    glm::vec3 closedScale{1.0f};    // mesh scale
    glm::vec3 colliderHalfExtents{0.5f};
    float closedYaw = 0.0f;         // group yaw in degrees
    float openYaw = 0.0f;           // target yaw when open
};
