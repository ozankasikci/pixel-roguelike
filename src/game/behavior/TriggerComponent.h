#pragma once

#include <glm/glm.hpp>
#include <cstdint>

enum class TriggerShape : uint8_t { Box, Sphere };

struct TriggerComponent {
    TriggerShape shape = TriggerShape::Box;
    glm::vec3 halfExtents{1.0f};   // for Box
    float radius = 1.0f;            // for Sphere
    bool fireOnce = false;
    bool playerInside = false;       // runtime state: is player currently inside
    bool pendingEnter = false;       // set by TriggerSystem, consumed by BehaviorSystem
    bool pendingExit = false;        // set by TriggerSystem, consumed by BehaviorSystem
    bool enabled = true;
};
