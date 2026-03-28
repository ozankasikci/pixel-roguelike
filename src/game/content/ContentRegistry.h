#pragma once

#include "game/prefabs/GameplayPrefabData.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "game/rendering/MaterialDefinition.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

struct WeaponDefinition {
    enum class Handedness {
        OneHanded,
        TwoHanded,
    };

    std::string id;
    std::string displayName;
    std::string slot;
    Handedness handedness = Handedness::OneHanded;
    std::string meshId;
    float equipWeight = 0.0f;
    std::string category;
    std::string description;
    float damage = 0.0f;
    float range = 0.0f;
    float cooldown = 0.0f;
};

using WeaponHandedness = WeaponDefinition::Handedness;

struct EnemyDefinition {
    std::string id;
    std::string displayName;
    std::string meshId;
    float maxHealth = 0.0f;
    float moveSpeed = 0.0f;
    float contactDamage = 0.0f;
};

struct ItemDefinition {
    std::string id;
    std::string displayName;
    std::string meshId;
    std::string pickupPrompt = "E  PICK UP";
};

struct SkillDefinition {
    std::string id;
    std::string displayName;
    std::string category;
    std::string description;
    int maxRank = 1;
};

enum class GameplayArchetypeKind {
    Checkpoint,
    DoubleDoor,
};

struct GameplayArchetypeDefinition {
    std::string id;
    GameplayArchetypeKind kind = GameplayArchetypeKind::Checkpoint;
    CheckpointSpawnSpec checkpoint;
    DoubleDoorSpawnSpec doubleDoor;
};

WeaponDefinition loadWeaponDefinitionAsset(const std::string& path);
EnemyDefinition loadEnemyDefinitionAsset(const std::string& path);
ItemDefinition loadItemDefinitionAsset(const std::string& path);
SkillDefinition loadSkillDefinitionAsset(const std::string& path);
GameplayArchetypeDefinition loadGameplayArchetypeAsset(const std::string& path);
MaterialDefinition loadMaterialDefinitionAsset(const std::string& path);
GameplayPrefabInstance instantiateGameplayArchetype(const GameplayArchetypeDefinition& definition,
                                                    const glm::vec3& position,
                                                    float yawDegrees = 0.0f);

class ContentRegistry {
public:
    void loadDefaults();

    const WeaponDefinition* findWeapon(const std::string& id) const;
    const EnemyDefinition* findEnemy(const std::string& id) const;
    const ItemDefinition* findItem(const std::string& id) const;
    const SkillDefinition* findSkill(const std::string& id) const;
    const GameplayArchetypeDefinition* findArchetype(const std::string& id) const;
    const MaterialDefinition* findMaterial(const std::string& id) const;
    const EnvironmentDefinition* findEnvironment(const std::string& id) const;
    const std::string* findEnvironmentPath(const std::string& id) const;
    const std::unordered_map<std::string, GameplayArchetypeDefinition>& archetypes() const { return archetypes_; }
    const std::unordered_map<std::string, MaterialDefinition>& materials() const { return materials_; }
    const std::unordered_map<std::string, EnvironmentDefinition>& environments() const { return environments_; }

private:
    std::unordered_map<std::string, WeaponDefinition> weapons_;
    std::unordered_map<std::string, EnemyDefinition> enemies_;
    std::unordered_map<std::string, ItemDefinition> items_;
    std::unordered_map<std::string, SkillDefinition> skills_;
    std::unordered_map<std::string, GameplayArchetypeDefinition> archetypes_;
    std::unordered_map<std::string, MaterialDefinition> materials_;
    std::unordered_map<std::string, EnvironmentDefinition> environments_;
    std::unordered_map<std::string, std::string> environmentPaths_;
};
