#pragma once

#include "game/prefabs/GameplayPrefabData.h"

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

struct CathedralSceneData {
    std::vector<CathedralMeshPlacement> meshes;
    std::vector<CathedralLightPlacement> lights;
    std::vector<CathedralBoxColliderPlacement> boxColliders;
    std::vector<CathedralCylinderColliderPlacement> cylinderColliders;
    CathedralPlayerSpawnPlacement playerSpawn;
    bool hasPlayerSpawn = false;
    std::vector<GameplayPrefabInstance> prefabs;
};

CathedralSceneData loadCathedralSceneData(const std::string& path);
