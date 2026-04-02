#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/TriggerComponent.h"
#include "game/rendering/EnvironmentProfile.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

struct BehaviorDeclaration {
    std::string eventType;   // "on_activate", "on_enter", "on_exit", "on_timer"
    ActionEntry action;
};

struct InteractableDeclaration {
    std::string promptText;
    float distance = 2.0f;
    float dotThreshold = 0.55f;
};

struct TriggerPlacement {
    TriggerShape shape = TriggerShape::Box;
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{1.0f};
    float radius = 1.0f;
    std::string nodeId;
    std::string parentNodeId;
    bool fireOnce = false;
    std::vector<BehaviorDeclaration> behaviors;
};

struct LevelMeshPlacement {
    std::string meshId;
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
    std::string materialId;
    std::optional<glm::vec3> tint;
    std::vector<BehaviorDeclaration> behaviors;
    std::optional<InteractableDeclaration> interactable;
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
    std::vector<BehaviorDeclaration> behaviors;
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

struct LevelGroupNode {
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
};

struct LevelDef {
    std::string environmentId = "neutral";
    EnvironmentProfile environmentProfile = EnvironmentProfile::Default;
    std::vector<LevelMeshPlacement> meshes;
    std::vector<LevelLightPlacement> lights;
    std::vector<LevelBoxColliderPlacement> boxColliders;
    std::vector<LevelCylinderColliderPlacement> cylinderColliders;
    LevelPlayerSpawn playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<LevelArchetypePlacement> archetypes;
    std::vector<LevelGroupNode> groups;
    std::vector<TriggerPlacement> triggers;
};

LevelDef loadLevelDef(const std::string& path);
LevelDef resolveLevelHierarchy(const LevelDef& data);
std::string serializeLevelDef(const LevelDef& data);
void saveLevelDef(const std::string& path, const LevelDef& data);
