#pragma once

#include <glm/glm.hpp>

#include <string>
#include <unordered_set>
#include <vector>

struct RunSession {
    struct OwnedWeaponEntry {
        std::string definitionId;
    };

    std::string currentLevelId = "cathedral";
    glm::vec3 respawnPosition{0.0f, 1.6f, 5.4f};
    float fallRespawnY = -8.0f;
    std::vector<OwnedWeaponEntry> ownedWeapons{
        {"old_dagger"},
        {"brigand_axe"},
    };
    std::string equippedRightHandWeaponId;
    std::string equippedLeftHandWeaponId;
    float maxEquipLoad = 12.0f;
    std::unordered_set<std::string> unlockedSkillIds;
    std::unordered_set<std::string> collectedItemIds;
};
