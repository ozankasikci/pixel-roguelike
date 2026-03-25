#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class LevelBuilder;
class Mesh;

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

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec);
entt::entity spawnDoubleDoor(LevelBuilder& builder,
                             Mesh* leftDoorMesh,
                             Mesh* rightDoorMesh,
                             const DoubleDoorSpawnSpec& spec);
