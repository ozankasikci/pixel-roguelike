#pragma once
#include <glm/glm.hpp>

enum class ColliderShape { Box, Cylinder };

struct StaticColliderComponent {
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 position{0.0f};       // world-space center of the collider
    glm::vec3 halfExtents{0.0f};    // for Box: half-sizes on each axis
    float radius = 0.0f;            // for Cylinder
    float halfHeight = 0.0f;        // for Cylinder
};
