#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct CathedralMeshPlacement {
    std::string meshName;
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 rotation{0.0f};
};

struct CathedralLightPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius = 1.0f;
    float intensity = 1.0f;
};

struct CathedralBoxColliderPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{0.0f};
};

struct CathedralCylinderColliderPlacement {
    glm::vec3 position{0.0f};
    float radius = 0.0f;
    float halfHeight = 0.0f;
};

struct CathedralPlayerSpawnPlacement {
    glm::vec3 position{0.0f};
    float fallRespawnY = -6.0f;
};

struct CathedralCheckpointPlacement {
    glm::vec3 position{0.0f};
    glm::vec3 respawnPosition{0.0f};
    float interactDistance = 2.4f;
    float interactDotThreshold = 0.55f;
    glm::vec3 lightPosition{0.0f};
    glm::vec3 lightColor{1.0f};
    float lightRadius = 1.0f;
    float lightIntensity = 1.0f;
};

struct CathedralDoubleDoorPlacement {
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

struct CathedralSceneData {
    std::vector<CathedralMeshPlacement> meshes;
    std::vector<CathedralLightPlacement> lights;
    std::vector<CathedralBoxColliderPlacement> boxColliders;
    std::vector<CathedralCylinderColliderPlacement> cylinderColliders;
    CathedralPlayerSpawnPlacement playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<CathedralCheckpointPlacement> checkpoints;
    std::vector<CathedralDoubleDoorPlacement> doors;
};

CathedralSceneData loadCathedralSceneData(const std::string& path);
