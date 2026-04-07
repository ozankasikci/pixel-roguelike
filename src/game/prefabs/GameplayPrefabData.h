#pragma once

#include <glm/glm.hpp>

enum class GameplayPrefabType {
    Checkpoint,
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

struct GameplayPrefabInstance {
    GameplayPrefabType type = GameplayPrefabType::Checkpoint;
    CheckpointSpawnSpec checkpoint;
};
