#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "game/behavior/ActionTypes.h"
#include "game/components/ColliderComponent.h"
#include "game/rendering/EnvironmentProfile.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

// Common fields shared by all placement structs (node graph metadata).
// Used as the parameter type for parser helpers — does not replace existing struct fields.
struct PlacementBase {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
};

struct BehaviorDeclaration {
    std::string eventType;   // "on_activate", "on_enter", "on_exit", "on_timer"
    ActionEntry action;
};

struct InteractableDeclaration {
    std::string promptText;
    float distance = 2.0f;
    float dotThreshold = 0.55f;
};

struct LevelColliderPlacement {
    ColliderShape shape  = ColliderShape::Box;
    ColliderMode  mode   = ColliderMode::Solid;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 halfExtents{1.0f};
    float     radius     = 1.0f;
    float     halfHeight = 1.0f;
    bool      fireOnce   = false;
    std::string nodeId;
    std::string parentNodeId;
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

struct LevelReflectionProbePlacement {
    glm::vec3 position{0.0f};
    glm::vec3 extents{4.0f, 3.0f, 4.0f};
    float blendDistance = 1.0f;
    float intensity = 1.0f;
    bool boxProjection = true;
    std::string nodeId;
    std::string parentNodeId;
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

struct LevelDoorPlacement {
    std::string name = "Door";
    glm::vec3 position{0.0f};
    float yawDegrees = 0.0f;
    float openAngle = 90.0f;
    float openDuration = 1.2f;
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    bool locked = false;
    std::string lockedPrompt = "E  This door is locked";
    std::string nodeId;
    std::string parentNodeId;
};

struct LevelDef {
    std::string environmentId = "neutral";
    EnvironmentProfile environmentProfile = EnvironmentProfile::Default;
    std::vector<LevelMeshPlacement> meshes;
    std::vector<LevelLightPlacement> lights;
    std::vector<LevelColliderPlacement> colliders;
    std::vector<LevelReflectionProbePlacement> reflectionProbes;
    LevelPlayerSpawn playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<LevelArchetypePlacement> archetypes;
    std::vector<LevelGroupNode> groups;
    std::vector<LevelDoorPlacement> doors;
};

LevelDef loadLevelDef(const std::string& path);
LevelDef resolveLevelHierarchy(const LevelDef& data);
std::string serializeLevelDef(const LevelDef& data);
void saveLevelDef(const std::string& path, const LevelDef& data);
