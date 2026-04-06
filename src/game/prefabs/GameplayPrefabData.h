#pragma once

#include <glm/glm.hpp>

#include <string>

enum class GameplayPrefabType {
    Checkpoint,
    DoubleDoor,
};

struct CheckpointSpawnSpec {
    glm::vec3 position{0.0f};
    glm::vec3 respawnPosition{0.0f};
    float interactDistance = 2.4f;
    float interactDotThreshold = 0.55f;
    glm::vec3 lightPosition{0.0f};
    glm::vec3 lightColor{1.0f};
    float lightRadius = 1.0f;
    float lightIntensity = 1.0f;
};

struct DoubleDoorSpawnSpec {
    std::string leftLeafMeshName = "door_leaf_left";
    std::string rightLeafMeshName = "door_leaf_right";
    glm::vec3 rootPosition{0.0f};
    glm::vec3 leftHingePosition{0.0f};
    glm::vec3 rightHingePosition{0.0f};
    glm::vec3 leafScale{1.0f};
    float closedYaw = 0.0f;
    float openAngle = 90.0f;
    float interactDistance = 3.0f;
    float interactDotThreshold = 0.72f;
    float openDuration = 2.4f;
};

struct GameplayPrefabInstance {
    GameplayPrefabType type = GameplayPrefabType::Checkpoint;
    CheckpointSpawnSpec checkpoint;
    DoubleDoorSpawnSpec doubleDoor;
};
