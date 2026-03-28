#include "game/content/ContentRegistry.h"

#include <cassert>
#include <cmath>
#include <filesystem>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
    return nearlyEqual(a.x, b.x, epsilon)
        && nearlyEqual(a.y, b.y, epsilon)
        && nearlyEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    const auto weapon = loadWeaponDefinitionAsset(WEAPON_DEF_FILE);
    assert(weapon.id == "old_dagger");
    assert(weapon.displayName == "Old_Dagger");
    assert(weapon.slot == "melee");
    assert(weapon.handedness == WeaponHandedness::OneHanded);
    assert(nearlyEqual(weapon.equipWeight, 2.0f));
    assert(weapon.category == "dagger");
    assert(weapon.description == "A_worn_blade_for_close_quarters");
    assert(nearlyEqual(weapon.damage, 12.0f));

    const auto twoHandedWeapon = loadWeaponDefinitionAsset(TWO_HANDED_WEAPON_DEF_FILE);
    assert(twoHandedWeapon.id == "brigand_axe");
    assert(twoHandedWeapon.handedness == WeaponHandedness::TwoHanded);
    assert(nearlyEqual(twoHandedWeapon.equipWeight, 6.5f));
    assert(twoHandedWeapon.category == "greataxe");
    assert(twoHandedWeapon.description == "A_heavy_raider_axe_built_for_committed_swings");

    const auto enemy = loadEnemyDefinitionAsset(ENEMY_DEF_FILE);
    assert(enemy.id == "sentinel");
    assert(nearlyEqual(enemy.maxHealth, 40.0f));

    const auto item = loadItemDefinitionAsset(ITEM_DEF_FILE);
    assert(item.id == "kindling_shard");
    assert(item.pickupPrompt == "E_PICK_UP_SHARD");

    const auto skill = loadSkillDefinitionAsset(SKILL_DEF_FILE);
    assert(skill.id == "cathedral_attunement");
    assert(skill.maxRank == 1);

    const auto checkpoint = loadGameplayArchetypeAsset(CHECKPOINT_ARCHETYPE_FILE);
    assert(checkpoint.id == "checkpoint_shrine");
    const auto checkpointInstance = instantiateGameplayArchetype(
        checkpoint,
        glm::vec3(0.0f, 0.0f, -35.3f),
        0.0f
    );
    assert(checkpointInstance.type == GameplayPrefabType::Checkpoint);
    assert(checkpointInstance.checkpoint.position == glm::vec3(0.0f, 1.3f, -35.3f));

    const auto door = loadGameplayArchetypeAsset(DOOR_ARCHETYPE_FILE);
    assert(door.id == "cathedral_double_door");
    const auto rotatedDoor = instantiateGameplayArchetype(
        door,
        glm::vec3(10.0f, 0.0f, 5.0f),
        90.0f
    );
    assert(rotatedDoor.type == GameplayPrefabType::DoubleDoor);
    assert(nearlyEqualVec3(rotatedDoor.doubleDoor.rootPosition, glm::vec3(10.0f, 3.13f, 5.0f)));

    ContentRegistry registry;
    registry.loadDefaults();
    const auto* oldDagger = registry.findWeapon("old_dagger");
    assert(oldDagger != nullptr);
    assert(oldDagger->handedness == WeaponHandedness::OneHanded);
    const auto* brigandAxe = registry.findWeapon("brigand_axe");
    assert(brigandAxe != nullptr);
    assert(brigandAxe->handedness == WeaponHandedness::TwoHanded);
    assert(registry.findEnemy("sentinel") != nullptr);
    assert(registry.findItem("kindling_shard") != nullptr);
    assert(registry.findSkill("cathedral_attunement") != nullptr);
    assert(registry.findArchetype("checkpoint_shrine") != nullptr);
    assert(registry.findMaterial("brick_default") != nullptr);
    assert(registry.findMaterial("brick_wall_old") != nullptr);
    assert(registry.findMaterial("cloister_stone") != nullptr);
    assert(registry.findEnvironment("neutral") != nullptr);
    assert(registry.findEnvironment("cloister_daylight") != nullptr);
    assert(registry.findEnvironmentPath("cloister_daylight") != nullptr);

    EnvironmentDefinition customEnvironment = makeEnvironmentDefinition(EnvironmentProfile::Neutral);
    customEnvironment.id = "editor_custom_test";
    customEnvironment.post.exposure = 1.23f;
    const auto customEnvironmentPath =
        std::filesystem::path(ENVIRONMENT_DIR) / "editor_custom_test.environment";
    saveEnvironmentDefinitionAsset(customEnvironmentPath.string(), customEnvironment);
    registry.loadDefaults();
    const auto* customLoaded = registry.findEnvironment("editor_custom_test");
    assert(customLoaded != nullptr);
    assert(nearlyEqual(customLoaded->post.exposure, 1.23f));
    const auto* customLoadedPath = registry.findEnvironmentPath("editor_custom_test");
    assert(customLoadedPath != nullptr);
    assert(std::filesystem::path(*customLoadedPath) == customEnvironmentPath);
    std::filesystem::remove(customEnvironmentPath);
    registry.loadDefaults();
    assert(registry.findEnvironment("editor_custom_test") == nullptr);

    const auto resolvedBrick = resolveMaterialDefinition("brick_wall_old", registry.materials());
    assert(resolvedBrick.shadingModel == MaterialKind::Brick);
    assert(nearlyEqualVec3(resolvedBrick.baseColor, glm::vec3(0.98f, 0.95f, 0.92f)));
    assert(nearlyEqual(resolvedBrick.uvScale.x, 0.16f));
    assert(nearlyEqual(resolvedBrick.uvScale.y, 0.16f));
    assert(resolvedBrick.uvMode == MaterialUvMode::WorldProjected);
    assert(resolvedBrick.proceduralSource == MaterialProceduralSource::GeneratedBrick);

    const auto resolvedStone = resolveMaterialDefinition("cloister_stone", registry.materials());
    assert(resolvedStone.shadingModel == MaterialKind::Stone);
    assert(resolvedStone.proceduralSource == MaterialProceduralSource::GeneratedStone);
    assert(nearlyEqual(resolvedStone.normalStrength, 0.34f));

    const auto* cloister = registry.findEnvironment("cloister_daylight");
    assert(cloister != nullptr);
    assert(cloister->id == "cloister_daylight");
    assert(cloister->sky.enabled);
    assert(cloister->lighting.sun.enabled);

    EnvironmentDefinition roundtrip;
    roundtrip.id = "editor_roundtrip_test";
    roundtrip.post.toneMapMode = 0;
    roundtrip.post.patternScale = 96.0f;
    roundtrip.post.depthViewScale = 0.133f;
    roundtrip.post.edgeThreshold = 0.27f;
    roundtrip.sky.enabled = true;
    roundtrip.sky.panoramaPath = "assets/skies/test_panorama.jpg";
    roundtrip.sky.panoramaTint = glm::vec3(0.91f, 0.87f, 0.79f);
    roundtrip.sky.panoramaStrength = 0.72f;
    roundtrip.sky.panoramaYawOffset = 24.0f;
    roundtrip.sky.cubemapFacePaths = {
        "assets/skies/test_cube/px.png",
        "assets/skies/test_cube/nx.png",
        "assets/skies/test_cube/py.png",
        "assets/skies/test_cube/ny.png",
        "assets/skies/test_cube/pz.png",
        "assets/skies/test_cube/nz.png",
    };
    roundtrip.sky.cubemapTint = glm::vec3(0.84f, 0.88f, 0.95f);
    roundtrip.sky.cubemapStrength = 0.61f;
    roundtrip.sky.cloudLayerAPath = "assets/skies/cloud_a.png";
    roundtrip.sky.cloudLayerBPath = "assets/skies/cloud_b.png";
    roundtrip.sky.horizonLayerPath = "assets/skies/horizon.png";
    roundtrip.lighting.sun.enabled = true;
    roundtrip.lighting.sun.intensity = 1.27f;

    const auto roundtripPath = std::filesystem::temp_directory_path() / "editor_roundtrip_test.environment";
    saveEnvironmentDefinitionAsset(roundtripPath.string(), roundtrip);
    const auto loadedRoundtrip = loadEnvironmentDefinitionAsset(roundtripPath.string());
    std::filesystem::remove(roundtripPath);

    assert(loadedRoundtrip.id == roundtrip.id);
    assert(loadedRoundtrip.post.toneMapMode == 0);
    assert(nearlyEqual(loadedRoundtrip.post.patternScale, 96.0f));
    assert(nearlyEqual(loadedRoundtrip.post.depthViewScale, 0.133f));
    assert(nearlyEqual(loadedRoundtrip.post.edgeThreshold, 0.27f));
    assert(loadedRoundtrip.sky.panoramaPath == roundtrip.sky.panoramaPath);
    assert(nearlyEqualVec3(loadedRoundtrip.sky.panoramaTint, roundtrip.sky.panoramaTint));
    assert(nearlyEqual(loadedRoundtrip.sky.panoramaStrength, roundtrip.sky.panoramaStrength));
    assert(nearlyEqual(loadedRoundtrip.sky.panoramaYawOffset, roundtrip.sky.panoramaYawOffset));
    assert(loadedRoundtrip.sky.cubemapFacePaths == roundtrip.sky.cubemapFacePaths);
    assert(nearlyEqualVec3(loadedRoundtrip.sky.cubemapTint, roundtrip.sky.cubemapTint));
    assert(nearlyEqual(loadedRoundtrip.sky.cubemapStrength, roundtrip.sky.cubemapStrength));
    assert(loadedRoundtrip.sky.cloudLayerAPath == roundtrip.sky.cloudLayerAPath);
    assert(loadedRoundtrip.sky.cloudLayerBPath == roundtrip.sky.cloudLayerBPath);
    assert(loadedRoundtrip.sky.horizonLayerPath == roundtrip.sky.horizonLayerPath);

    return 0;
}
