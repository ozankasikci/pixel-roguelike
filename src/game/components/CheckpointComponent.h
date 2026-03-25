#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct CheckpointComponent {
    glm::vec3 respawnPosition{0.0f};
    float interactDistance = 2.4f;
    float interactDotThreshold = 0.55f;
    bool active = false;
    entt::entity lightEntity = entt::null;
};
