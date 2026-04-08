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
    float openYaw = 0.0f;           // Target yaw when fully open
    entt::entity colliderEntity = entt::null;  // Linked collider (set at init)
    glm::vec3 colliderLocalOffset{0.0f};       // Collider position in leaf local space
};
