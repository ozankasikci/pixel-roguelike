#pragma once

#include <glm/glm.hpp>

struct PlayerLockParams {
    float duration = 0.0f;
};

struct TeleportPlayerParams {
    glm::vec3 position{0.0f};
};
