#pragma once

#include <entt/entt.hpp>
#include <string>

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
