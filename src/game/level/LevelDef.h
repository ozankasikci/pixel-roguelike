#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "game/rendering/MaterialKind.h"
#include "game/rendering/EnvironmentProfile.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

struct LevelMeshPlacement {
    std::string meshId;
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    std::string materialId;
    std::optional<MaterialKind> material;
    std::optional<glm::vec3> tint;
};

struct LevelLightPlacement {
    LightType type = LightType::Point;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    std::string nodeId;
    std::string parentNodeId;
    glm::vec3 color{1.0f};
    float radius = 1.0f;
    float intensity = 1.0f;
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 30.0f;
    bool castsShadows = false;
};

struct LevelBoxColliderPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    glm::vec3 halfExtents{0.0f};
};

struct LevelCylinderColliderPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    float radius = 0.0f;
    float halfHeight = 0.0f;
};

struct LevelPlayerSpawn {
    glm::vec3 position{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    float fallRespawnY = -8.0f;
};

struct LevelArchetypePlacement {
    std::string archetypeId;
    glm::vec3 position{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    float yawDegrees = 0.0f;
};

struct LevelDef {
    std::string environmentId = "neutral";
    EnvironmentProfile environmentProfile = EnvironmentProfile::Neutral;
    std::vector<LevelMeshPlacement> meshes;
    std::vector<LevelLightPlacement> lights;
    std::vector<LevelBoxColliderPlacement> boxColliders;
    std::vector<LevelCylinderColliderPlacement> cylinderColliders;
    LevelPlayerSpawn playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<LevelArchetypePlacement> archetypes;
};

LevelDef loadLevelDef(const std::string& path);
LevelDef resolveLevelHierarchy(const LevelDef& data);
std::string serializeLevelDef(const LevelDef& data);
void saveLevelDef(const std::string& path, const LevelDef& data);
