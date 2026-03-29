#include "game/level/LevelDef.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    namespace fs = std::filesystem;

    LevelDef level;
    level.environmentId = "cloister_daylight";
    level.environmentProfile = EnvironmentProfile::CloisterDaylight;
    level.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(1.0f, 2.0f, 3.0f),
        .scale = glm::vec3(4.0f, 5.0f, 6.0f),
        .rotation = glm::vec3(7.0f, 8.0f, 9.0f),
        .nodeId = "root_mesh",
        .materialId = "brick_default",
        .material = MaterialKind::Brick,
        .tint = glm::vec3(0.4f, 0.5f, 0.6f),
    });
    level.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(1.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::vec3(0.0f),
        .nodeId = "child_mesh",
        .parentNodeId = "root_mesh",
        .materialId = "stone_default",
        .material = MaterialKind::Stone,
    });
    level.lights.push_back(LevelLightPlacement{
        .type = LightType::Spot,
        .position = glm::vec3(0.0f, 3.0f, 4.0f),
        .direction = glm::vec3(0.0f, -1.0f, 0.0f),
        .nodeId = "spot_1",
        .color = glm::vec3(1.0f, 0.8f, 0.6f),
        .radius = 12.0f,
        .intensity = 0.9f,
        .innerConeDegrees = 22.0f,
        .outerConeDegrees = 34.0f,
        .castsShadows = true,
    });
    level.boxColliders.push_back(LevelBoxColliderPlacement{
        .position = glm::vec3(5.0f, 0.5f, -2.0f),
        .rotation = glm::vec3(0.0f, 15.0f, 0.0f),
        .nodeId = "box_1",
        .halfExtents = glm::vec3(1.0f, 2.0f, 3.0f),
    });
    level.cylinderColliders.push_back(LevelCylinderColliderPlacement{
        .position = glm::vec3(-3.0f, 1.0f, 2.0f),
        .rotation = glm::vec3(0.0f, 30.0f, 0.0f),
        .nodeId = "cyl_1",
        .radius = 0.8f,
        .halfHeight = 1.6f,
    });
    level.playerSpawn = LevelPlayerSpawn{
        .position = glm::vec3(0.0f, 1.6f, 6.0f),
        .nodeId = "spawn_1",
        .fallRespawnY = -10.0f,
    };
    level.hasPlayerSpawn = true;
    level.archetypes.push_back(LevelArchetypePlacement{
        .archetypeId = "checkpoint_shrine",
        .position = glm::vec3(2.0f, 0.0f, -6.0f),
        .nodeId = "checkpoint_1",
        .yawDegrees = 45.0f,
    });

    const std::string serialized = serializeLevelDef(level);
    assert(serialized.find("environment_profile cloister_daylight") != std::string::npos);
    assert(serialized.find("material brick_default tint 0.4 0.5 0.6") != std::string::npos);
    assert(serialized.find("node root_mesh") != std::string::npos);
    assert(serialized.find("parent root_mesh") != std::string::npos);
    assert(serialized.find("rotation 0.0 15.0 0.0") != std::string::npos);

    const fs::path tempPath = test_support::tempPath("gsd_level_roundtrip.scene");
    saveLevelDef(tempPath.string(), level);
    const LevelDef loaded = loadLevelDef(tempPath.string());
    fs::remove(tempPath);

    assert(loaded.environmentId == "cloister_daylight");
    assert(loaded.environmentProfile == EnvironmentProfile::CloisterDaylight);
    assert(loaded.meshes.size() == 2);
    assert(loaded.meshes.front().materialId == "brick_default");
    assert(loaded.meshes.front().tint.has_value());
    assert(*loaded.meshes.front().tint == glm::vec3(0.4f, 0.5f, 0.6f));
    assert(loaded.meshes[1].parentNodeId == "root_mesh");
    assert(loaded.boxColliders.front().rotation == glm::vec3(0.0f, 15.0f, 0.0f));
    assert(loaded.lights.size() == 1);
    assert(loaded.lights.front().type == LightType::Spot);
    assert(loaded.lights.front().castsShadows);
    assert(loaded.boxColliders.size() == 1);
    assert(loaded.cylinderColliders.size() == 1);
    assert(loaded.hasPlayerSpawn);
    assert(loaded.archetypes.size() == 1);

    LevelDef hierarchy;
    hierarchy.environmentId = "neutral";
    hierarchy.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(10.0f, 0.0f, 0.0f),
        .scale = glm::vec3(2.0f),
        .rotation = glm::vec3(0.0f, 90.0f, 0.0f),
        .nodeId = "parent",
        .materialId = "stone_default",
        .material = MaterialKind::Stone,
    });
    hierarchy.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(1.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::vec3(0.0f),
        .nodeId = "child",
        .parentNodeId = "parent",
        .materialId = "stone_default",
        .material = MaterialKind::Stone,
    });
    const LevelDef resolved = resolveLevelHierarchy(hierarchy);
    assert(resolved.meshes.size() == 2);
    assert(glm::distance(resolved.meshes[1].position, glm::vec3(10.0f, 0.0f, -2.0f)) < 0.001f);

    LevelDef gameReadyLevel;
    gameReadyLevel.environmentId = "game_ready_neutral";
    gameReadyLevel.environmentProfile = EnvironmentProfile::GameReadyNeutral;
    const std::string gameReadySerialized = serializeLevelDef(gameReadyLevel);
    assert(gameReadySerialized.find("environment_profile game_ready_neutral") != std::string::npos);

    const fs::path gameReadyPath = test_support::tempPath("gsd_game_ready_level.scene");
    saveLevelDef(gameReadyPath.string(), gameReadyLevel);
    const LevelDef loadedGameReady = loadLevelDef(gameReadyPath.string());
    fs::remove(gameReadyPath);
    assert(loadedGameReady.environmentId == "game_ready_neutral");
    assert(loadedGameReady.environmentProfile == EnvironmentProfile::GameReadyNeutral);

    return 0;
}
