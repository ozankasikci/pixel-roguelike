#pragma once

#include <glm/glm.hpp>

struct DoorLeafComponent {
    glm::vec3 hingePosition{0.0f};
    glm::vec3 centerOffsetFromHinge{0.0f};
    glm::vec3 closedScale{1.0f};
    glm::vec3 colliderHalfExtents{0.5f};
    float closedYaw = 0.0f;
    float openYaw = 0.0f;
};
