#pragma once

#include <glm/glm.hpp>

struct PlayerSpawnComponent {
    glm::vec3 respawnPosition{0.0f};
    float fallRespawnY = -6.0f;
};
