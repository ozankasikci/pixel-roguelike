#pragma once

#include <glm/glm.hpp>

#include <string>
#include <unordered_set>
#include <vector>

struct RunSession {
    struct LoadoutSlot {
        std::string definitionId;
        bool equipped = false;
    };

    std::string currentLevelId = "cathedral";
    glm::vec3 respawnPosition{0.0f, 1.6f, 5.4f};
    float fallRespawnY = -8.0f;
    std::vector<LoadoutSlot> loadout;
    std::unordered_set<std::string> unlockedSkillIds;
    std::unordered_set<std::string> collectedItemIds;
};
