#include "game/level/LevelDef.h"

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
        .materialId = "brick_default",
        .material = MaterialKind::Brick,
        .tint = glm::vec3(0.4f, 0.5f, 0.6f),
    });
    level.lights.push_back(LevelLightPlacement{
        .type = LightType::Spot,
        .position = glm::vec3(0.0f, 3.0f, 4.0f),
        .direction = glm::vec3(0.0f, -1.0f, 0.0f),
        .color = glm::vec3(1.0f, 0.8f, 0.6f),
        .radius = 12.0f,
        .intensity = 0.9f,
        .innerConeDegrees = 22.0f,
        .outerConeDegrees = 34.0f,
        .castsShadows = true,
    });
    level.boxColliders.push_back(LevelBoxColliderPlacement{glm::vec3(5.0f, 0.5f, -2.0f), glm::vec3(1.0f, 2.0f, 3.0f)});
    level.cylinderColliders.push_back(LevelCylinderColliderPlacement{glm::vec3(-3.0f, 1.0f, 2.0f), 0.8f, 1.6f});
    level.playerSpawn = LevelPlayerSpawn{glm::vec3(0.0f, 1.6f, 6.0f), -10.0f};
    level.hasPlayerSpawn = true;
    level.archetypes.push_back(LevelArchetypePlacement{"checkpoint_shrine", glm::vec3(2.0f, 0.0f, -6.0f), 45.0f});

    const std::string serialized = serializeLevelDef(level);
    assert(serialized.find("environment_profile cloister_daylight") != std::string::npos);
    assert(serialized.find("material brick_default tint 0.4 0.5 0.6") != std::string::npos);

    const fs::path tempPath = fs::temp_directory_path() / "gsd_level_roundtrip.scene";
    saveLevelDef(tempPath.string(), level);
    const LevelDef loaded = loadLevelDef(tempPath.string());
    fs::remove(tempPath);

    assert(loaded.environmentId == "cloister_daylight");
    assert(loaded.environmentProfile == EnvironmentProfile::CloisterDaylight);
    assert(loaded.meshes.size() == 1);
    assert(loaded.meshes.front().materialId == "brick_default");
    assert(loaded.meshes.front().tint.has_value());
    assert(*loaded.meshes.front().tint == glm::vec3(0.4f, 0.5f, 0.6f));
    assert(loaded.lights.size() == 1);
    assert(loaded.lights.front().type == LightType::Spot);
    assert(loaded.lights.front().castsShadows);
    assert(loaded.boxColliders.size() == 1);
    assert(loaded.cylinderColliders.size() == 1);
    assert(loaded.hasPlayerSpawn);
    assert(loaded.archetypes.size() == 1);
    return 0;
}
