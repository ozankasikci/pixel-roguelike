#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>

enum class ColliderShape : uint8_t { Box, Sphere, Cylinder, Capsule };
enum class ColliderMode  : uint8_t { Solid, Trigger, SolidAndTrigger };

struct ColliderComponent {
    ColliderShape shape = ColliderShape::Box;
    ColliderMode  mode  = ColliderMode::Solid;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 halfExtents{1.0f};
    float     radius     = 1.0f;
    float     halfHeight = 1.0f;
    bool fireOnce     = false;
    bool enabled      = true;
    bool playerInside = false;
    bool pendingEnter = false;
    bool pendingExit  = false;
    std::string parentNodeId;  // scene-file parent node (for linking colliders to door leaves)
};
