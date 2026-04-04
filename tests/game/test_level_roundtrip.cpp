#include "game/level/LevelDef.h"
#include "game/components/ColliderComponent.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    namespace fs = std::filesystem;

    LevelDef level;
    level.environmentId = "cloister_daylight";
    level.environmentProfile = EnvironmentProfile::Default;
    level.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(1.0f, 2.0f, 3.0f),
        .scale = glm::vec3(4.0f, 5.0f, 6.0f),
        .rotation = glm::vec3(7.0f, 8.0f, 9.0f),
        .nodeId = "root_mesh",
        .materialId = "brick_default",
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
    level.colliders.push_back(LevelColliderPlacement{
        .shape = ColliderShape::Box,
        .mode = ColliderMode::Solid,
        .position = glm::vec3(5.0f, 0.5f, -2.0f),
        .rotation = glm::vec3(0.0f, 15.0f, 0.0f),
        .halfExtents = glm::vec3(1.0f, 2.0f, 3.0f),
        .nodeId = "box_1",
    });
    level.colliders.push_back(LevelColliderPlacement{
        .shape = ColliderShape::Cylinder,
        .mode = ColliderMode::Solid,
        .position = glm::vec3(-3.0f, 1.0f, 2.0f),
        .rotation = glm::vec3(0.0f, 30.0f, 0.0f),
        .radius = 0.8f,
        .halfHeight = 1.6f,
        .nodeId = "cyl_1",
    });
    level.colliders.push_back(LevelColliderPlacement{
        .shape = ColliderShape::Sphere,
        .mode = ColliderMode::Trigger,
        .position = glm::vec3(0.0f, 1.0f, 5.0f),
        .radius = 2.0f,
        .fireOnce = true,
        .nodeId = "sphere_trigger_1",
    });
    level.colliders.push_back(LevelColliderPlacement{
        .shape = ColliderShape::Capsule,
        .mode = ColliderMode::SolidAndTrigger,
        .position = glm::vec3(1.0f, 0.0f, 0.0f),
        .radius = 0.3f,
        .halfHeight = 0.9f,
        .nodeId = "capsule_1",
    });
    level.reflectionProbes.push_back(LevelReflectionProbePlacement{
        .position = glm::vec3(7.0f, 2.0f, -4.0f),
        .extents = glm::vec3(5.0f, 3.0f, 6.0f),
        .blendDistance = 1.4f,
        .intensity = 0.85f,
        .boxProjection = false,
        .nodeId = "probe_1",
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
    assert(serialized.find("reflection_probe 7.0 2.0 -4.0 5.0 3.0 6.0 1.4 0.85 false") != std::string::npos);
    // Verify new unified collider format
    assert(serialized.find("collider box solid") != std::string::npos);
    assert(serialized.find("collider cylinder solid") != std::string::npos);
    assert(serialized.find("collider sphere trigger") != std::string::npos);
    assert(serialized.find("collider capsule solidandtrigger") != std::string::npos);
    // Verify old formats are NOT present
    assert(serialized.find("collider_box") == std::string::npos);
    assert(serialized.find("collider_cylinder") == std::string::npos);
    assert(serialized.find("trigger_box") == std::string::npos);
    assert(serialized.find("trigger_sphere") == std::string::npos);

    const fs::path tempPath = test_support::tempPath("gsd_level_roundtrip.scene");
    saveLevelDef(tempPath.string(), level);
    const LevelDef loaded = loadLevelDef(tempPath.string());
    fs::remove(tempPath);

    assert(loaded.environmentId == "cloister_daylight");
    assert(loaded.environmentProfile == EnvironmentProfile::Default);
    assert(loaded.meshes.size() == 2);
    assert(loaded.meshes.front().materialId == "brick_default");
    assert(loaded.meshes.front().tint.has_value());
    assert(*loaded.meshes.front().tint == glm::vec3(0.4f, 0.5f, 0.6f));
    assert(loaded.meshes[1].parentNodeId == "root_mesh");
    assert(loaded.lights.size() == 1);
    assert(loaded.lights.front().type == LightType::Spot);
    assert(loaded.lights.front().castsShadows);
    assert(loaded.colliders.size() == 4);
    // Box collider
    assert(loaded.colliders[0].shape == ColliderShape::Box);
    assert(loaded.colliders[0].mode == ColliderMode::Solid);
    assert(test_support::nearlyEqualVec3(loaded.colliders[0].rotation, glm::vec3(0.0f, 15.0f, 0.0f)));
    assert(loaded.colliders[0].nodeId == "box_1");
    // Cylinder collider
    assert(loaded.colliders[1].shape == ColliderShape::Cylinder);
    assert(loaded.colliders[1].mode == ColliderMode::Solid);
    assert(test_support::nearlyEqual(loaded.colliders[1].radius, 0.8f));
    assert(test_support::nearlyEqual(loaded.colliders[1].halfHeight, 1.6f));
    assert(loaded.colliders[1].nodeId == "cyl_1");
    // Sphere trigger
    assert(loaded.colliders[2].shape == ColliderShape::Sphere);
    assert(loaded.colliders[2].mode == ColliderMode::Trigger);
    assert(loaded.colliders[2].fireOnce);
    assert(loaded.colliders[2].nodeId == "sphere_trigger_1");
    // Capsule solidandtrigger
    assert(loaded.colliders[3].shape == ColliderShape::Capsule);
    assert(loaded.colliders[3].mode == ColliderMode::SolidAndTrigger);
    assert(loaded.reflectionProbes.size() == 1);
    assert(loaded.reflectionProbes.front().nodeId == "probe_1");
    assert(test_support::nearlyEqualVec3(loaded.reflectionProbes.front().extents, glm::vec3(5.0f, 3.0f, 6.0f)));
    assert(test_support::nearlyEqual(loaded.reflectionProbes.front().blendDistance, 1.4f));
    assert(test_support::nearlyEqual(loaded.reflectionProbes.front().intensity, 0.85f));
    assert(!loaded.reflectionProbes.front().boxProjection);
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
    });
    hierarchy.meshes.push_back(LevelMeshPlacement{
        .meshId = "cube",
        .position = glm::vec3(1.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f),
        .rotation = glm::vec3(0.0f),
        .nodeId = "child",
        .parentNodeId = "parent",
        .materialId = "stone_default",
    });
    const LevelDef resolved = resolveLevelHierarchy(hierarchy);
    assert(resolved.meshes.size() == 2);
    assert(glm::distance(resolved.meshes[1].position, glm::vec3(10.0f, 0.0f, -2.0f)) < 0.001f);

    LevelDef defaultLevel;
    defaultLevel.environmentId = "game_ready_neutral";
    defaultLevel.environmentProfile = EnvironmentProfile::Default;
    const std::string defaultSerialized = serializeLevelDef(defaultLevel);
    assert(defaultSerialized.find("environment_profile game_ready_neutral") != std::string::npos);

    const fs::path defaultPath = test_support::tempPath("gsd_default_level.scene");
    saveLevelDef(defaultPath.string(), defaultLevel);
    const LevelDef loadedDefault = loadLevelDef(defaultPath.string());
    fs::remove(defaultPath);
    assert(loadedDefault.environmentId == "game_ready_neutral");
    assert(loadedDefault.environmentProfile == EnvironmentProfile::Default);

    return 0;
}
