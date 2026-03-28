#include "game/content/ContentRegistry.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>

int main() {
    const auto weapon = loadWeaponDefinitionAsset(WEAPON_DEF_FILE);
    assert(weapon.id == "old_dagger");
    assert(weapon.displayName == "Old_Dagger");
    assert(weapon.slot == "melee");
    assert(weapon.handedness == WeaponHandedness::OneHanded);
    assert(test_support::nearlyEqual(weapon.equipWeight, 2.0f));
    assert(weapon.category == "dagger");
    assert(weapon.description == "A_worn_blade_for_close_quarters");
    assert(test_support::nearlyEqual(weapon.damage, 12.0f));

    const auto twoHandedWeapon = loadWeaponDefinitionAsset(TWO_HANDED_WEAPON_DEF_FILE);
    assert(twoHandedWeapon.id == "brigand_axe");
    assert(twoHandedWeapon.handedness == WeaponHandedness::TwoHanded);
    assert(test_support::nearlyEqual(twoHandedWeapon.equipWeight, 6.5f));
    assert(twoHandedWeapon.category == "greataxe");
    assert(twoHandedWeapon.description == "A_heavy_raider_axe_built_for_committed_swings");

    const auto enemy = loadEnemyDefinitionAsset(ENEMY_DEF_FILE);
    assert(enemy.id == "sentinel");
    assert(test_support::nearlyEqual(enemy.maxHealth, 40.0f));

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
    assert(test_support::nearlyEqualVec3(rotatedDoor.doubleDoor.rootPosition, glm::vec3(10.0f, 3.13f, 5.0f)));

    {
        GameplayArchetypeDefinition prefabRoundtrip = door;
        prefabRoundtrip.id = "editor_roundtrip_prefab";
        prefabRoundtrip.doubleDoor.openDuration = 3.7f;
        const auto prefabPath = test_support::tempPath("editor_roundtrip_prefab.prefab");
        saveGameplayArchetypeAsset(prefabPath.string(), prefabRoundtrip);
        const auto loadedPrefab = loadGameplayArchetypeAsset(prefabPath.string());
        std::filesystem::remove(prefabPath);
        assert(loadedPrefab.id == prefabRoundtrip.id);
        assert(loadedPrefab.kind == prefabRoundtrip.kind);
        assert(loadedPrefab.doubleDoor.leftLeafMeshName == prefabRoundtrip.doubleDoor.leftLeafMeshName);
        assert(test_support::nearlyEqual(loadedPrefab.doubleDoor.openDuration, prefabRoundtrip.doubleDoor.openDuration));
        assert(test_support::nearlyEqualVec3(loadedPrefab.doubleDoor.leafScale, prefabRoundtrip.doubleDoor.leafScale));
    }

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
    assert(test_support::nearlyEqual(customLoaded->post.exposure, 1.23f));
    const auto* customLoadedPath = registry.findEnvironmentPath("editor_custom_test");
    assert(customLoadedPath != nullptr);
    assert(std::filesystem::path(*customLoadedPath) == customEnvironmentPath);
    std::filesystem::remove(customEnvironmentPath);
    registry.loadDefaults();
    assert(registry.findEnvironment("editor_custom_test") == nullptr);

    const auto resolvedBrick = resolveMaterialDefinition("brick_wall_old", registry.materials());
    assert(resolvedBrick.shadingModel == MaterialKind::Brick);
    assert(test_support::nearlyEqualVec3(resolvedBrick.baseColor, glm::vec3(0.98f, 0.95f, 0.92f)));
    assert(test_support::nearlyEqual(resolvedBrick.uvScale.x, 0.16f));
    assert(test_support::nearlyEqual(resolvedBrick.uvScale.y, 0.16f));
    assert(resolvedBrick.uvMode == MaterialUvMode::WorldProjected);
    assert(resolvedBrick.proceduralSource == MaterialProceduralSource::GeneratedBrick);

    const auto resolvedStone = resolveMaterialDefinition("cloister_stone", registry.materials());
    assert(resolvedStone.shadingModel == MaterialKind::Stone);
    assert(resolvedStone.proceduralSource == MaterialProceduralSource::GeneratedStone);
    assert(test_support::nearlyEqual(resolvedStone.normalStrength, 0.34f));

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

    const auto roundtripPath = test_support::tempPath("editor_roundtrip_test.environment");
    saveEnvironmentDefinitionAsset(roundtripPath.string(), roundtrip);
    const auto loadedRoundtrip = loadEnvironmentDefinitionAsset(roundtripPath.string());
    std::filesystem::remove(roundtripPath);

    assert(loadedRoundtrip.id == roundtrip.id);
    assert(loadedRoundtrip.post.toneMapMode == 0);
    assert(test_support::nearlyEqual(loadedRoundtrip.post.patternScale, 96.0f));
    assert(test_support::nearlyEqual(loadedRoundtrip.post.depthViewScale, 0.133f));
    assert(test_support::nearlyEqual(loadedRoundtrip.post.edgeThreshold, 0.27f));
    assert(loadedRoundtrip.sky.panoramaPath == roundtrip.sky.panoramaPath);
    assert(test_support::nearlyEqualVec3(loadedRoundtrip.sky.panoramaTint, roundtrip.sky.panoramaTint));
    assert(test_support::nearlyEqual(loadedRoundtrip.sky.panoramaStrength, roundtrip.sky.panoramaStrength));
    assert(test_support::nearlyEqual(loadedRoundtrip.sky.panoramaYawOffset, roundtrip.sky.panoramaYawOffset));
    assert(loadedRoundtrip.sky.cubemapFacePaths == roundtrip.sky.cubemapFacePaths);
    assert(test_support::nearlyEqualVec3(loadedRoundtrip.sky.cubemapTint, roundtrip.sky.cubemapTint));
    assert(test_support::nearlyEqual(loadedRoundtrip.sky.cubemapStrength, roundtrip.sky.cubemapStrength));
    assert(loadedRoundtrip.sky.cloudLayerAPath == roundtrip.sky.cloudLayerAPath);
    assert(loadedRoundtrip.sky.cloudLayerBPath == roundtrip.sky.cloudLayerBPath);
    assert(loadedRoundtrip.sky.horizonLayerPath == roundtrip.sky.horizonLayerPath);

    return 0;
}
