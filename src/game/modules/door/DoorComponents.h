#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <cstdint>

// Consolidated door component header (per D-10: all door components in one file).

// ---------------------------------------------------------------------------
// DoorConfigComponent  — configuration for an interactive door root entity
// ---------------------------------------------------------------------------
struct DoorConfigComponent {
    entt::entity leftLeaf = entt::null;
    entt::entity rightLeaf = entt::null;
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    float openDuration = 1.2f;
    float openAngle = 90.0f;
    bool locked = false;
    std::string lockedPrompt = "E  This door is locked";
};

// ---------------------------------------------------------------------------
// DoorStateComponent  — runtime animation state for an interactive door
// ---------------------------------------------------------------------------
enum class DoorTargetState : uint8_t { Closed, Open };

struct DoorStateComponent {
    float progress = 0.0f;
    DoorTargetState targetState = DoorTargetState::Closed;
};

// Free-function helpers (POD rule: no methods on components)
inline bool isDoorFullyClosed(const DoorStateComponent& s) {
    return s.targetState == DoorTargetState::Closed && s.progress <= 0.0f;
}
inline bool isDoorFullyOpen(const DoorStateComponent& s) {
    return s.targetState == DoorTargetState::Open && s.progress >= 1.0f;
}
inline bool isDoorMoving(const DoorStateComponent& s) {
    return (s.targetState == DoorTargetState::Open && s.progress < 1.0f) ||
           (s.targetState == DoorTargetState::Closed && s.progress > 0.0f);
}

// ---------------------------------------------------------------------------
// DoorLeafComponent  — per-leaf swing animation data
// ---------------------------------------------------------------------------
struct DoorLeafComponent {
    glm::vec3 basePosition{0.0f};   // world position of the door group
    glm::vec3 pivot{0.0f};          // mesh-local hinge offset
    glm::vec3 meshCenter{0.0f};     // AABB center of the door mesh (for frame alignment)
    glm::vec3 closedScale{1.0f};    // mesh scale
    float closedYaw = 0.0f;         // group yaw in degrees
    float openYaw = 0.0f;           // target yaw when open
};
