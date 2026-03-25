#include "game/content/ContentRegistry.h"

#include <cassert>
#include <cmath>

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
    assert(weapon.slot == "melee");
    assert(nearlyEqual(weapon.damage, 12.0f));

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
    assert(registry.findWeapon("old_dagger") != nullptr);
    assert(registry.findEnemy("sentinel") != nullptr);
    assert(registry.findItem("kindling_shard") != nullptr);
    assert(registry.findSkill("cathedral_attunement") != nullptr);
    assert(registry.findArchetype("checkpoint_shrine") != nullptr);

    return 0;
}
