#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct LevelMeshPlacement {
    std::string meshId;
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotation{0.0f};
};

struct LevelLightPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius = 1.0f;
    float intensity = 1.0f;
};

struct LevelBoxColliderPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{0.0f};
};

struct LevelCylinderColliderPlacement {
    glm::vec3 position{0.0f};
    float radius = 0.0f;
    float halfHeight = 0.0f;
};

struct LevelPlayerSpawn {
    glm::vec3 position{0.0f};
    float fallRespawnY = -8.0f;
};

struct LevelArchetypePlacement {
    std::string archetypeId;
    glm::vec3 position{0.0f};
    float yawDegrees = 0.0f;
};

struct LevelDef {
    std::vector<LevelMeshPlacement> meshes;
    std::vector<LevelLightPlacement> lights;
    std::vector<LevelBoxColliderPlacement> boxColliders;
    std::vector<LevelCylinderColliderPlacement> cylinderColliders;
    LevelPlayerSpawn playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<LevelArchetypePlacement> archetypes;
};

LevelDef loadLevelDef(const std::string& path);
