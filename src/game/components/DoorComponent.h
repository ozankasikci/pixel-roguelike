#pragma once

#include <entt/entt.hpp>

struct DoorComponent {
    entt::entity leftLeaf = entt::null;
    entt::entity rightLeaf = entt::null;
    float interactDistance = 3.0f;
    float interactDotThreshold = 0.72f;
    float openDuration = 2.4f;
    float progress = 0.0f;
    bool opening = false;
    bool opened = false;
};
