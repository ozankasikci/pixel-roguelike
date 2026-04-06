#pragma once

#include <glm/glm.hpp>

#include <string>

enum class GameplayPrefabType {
    Checkpoint,
    DoubleDoor,
};

struct SingleDoorSpawnSpec {
    std::string doorMeshName = "SM_DoorA";
    std::string frameMeshName = "SM_FrameA";
    std::string doorMaterialId = "qdp_door_a";
    std::string frameMaterialId = "stone_default";
    glm::vec3 rootPosition{0.0f};      // world position of door frame center (floor level)
    float doorYawDegrees = 0.0f;       // rotation of entire assembly around Y
    float openAngle = 90.0f;           // how far the door swings open (degrees)
    float openDuration = 1.2f;         // seconds for open animation
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    bool locked = false;               // if true, show locked prompt, don't open
    std::string lockedPrompt = "E  This door is locked";
    glm::vec3 doorTint{1.0f};
    glm::vec3 frameTint{1.0f};
    glm::vec3 hingePivot{-0.45f, 0.0f, 0.04f};
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
