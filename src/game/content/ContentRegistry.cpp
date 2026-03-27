#include "game/content/ContentRegistry.h"
#include "engine/core/PathUtils.h"

#include <cctype>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

[[noreturn]] void throwParseError(const std::string& path, int lineNumber, const std::string& message) {
    throw std::runtime_error(path + ":" + std::to_string(lineNumber) + ": " + message);
}

bool isCommentOrEmpty(const std::string& line) {
    for (char c : line) {
        if (c == '#') {
            return true;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> tokenizeRecord(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

template<typename Def>
Def loadDefinitionFile(const std::string& path, const std::function<void(Def&, const std::vector<std::string>&, int)>& apply) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open content definition: " + path);
    }

    Def definition;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }
        const auto tokens = tokenizeRecord(line);
        if (tokens.empty()) {
            continue;
        }
        apply(definition, tokens, lineNumber);
    }
    return definition;
}

glm::vec3 rotateYaw(const glm::vec3& value, float yawDegrees) {
    const float radians = yawDegrees * 3.14159265358979323846f / 180.0f;
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    return glm::vec3(
        value.x * c - value.z * s,
        value.y,
        value.x * s + value.z * c
    );
}

glm::vec3 transformPoint(const glm::vec3& local, const glm::vec3& translation, float yawDegrees) {
    return translation + rotateYaw(local, yawDegrees);
}

} // namespace

WeaponDefinition loadWeaponDefinitionAsset(const std::string& path) {
    return loadDefinitionFile<WeaponDefinition>(path, [&](WeaponDefinition& definition, const std::vector<std::string>& tokens, int lineNumber) {
        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            return;
        }
        if (key == "display_name" && tokens.size() == 2) {
            definition.displayName = tokens[1];
            return;
        }
        if (key == "slot" && tokens.size() == 2) {
            definition.slot = tokens[1];
            return;
        }
        if (key == "handedness" && tokens.size() == 2) {
            if (tokens[1] == "one_handed") {
                definition.handedness = WeaponDefinition::Handedness::OneHanded;
                return;
            }
            if (tokens[1] == "two_handed") {
                definition.handedness = WeaponDefinition::Handedness::TwoHanded;
                return;
            }
            throwParseError(path, lineNumber, "unknown weapon handedness");
        }
        if (key == "mesh" && tokens.size() == 2) {
            definition.meshId = tokens[1];
            return;
        }
        if (key == "equip_weight" && tokens.size() == 2) {
            definition.equipWeight = std::stof(tokens[1]);
            return;
        }
        if (key == "category" && tokens.size() == 2) {
            definition.category = tokens[1];
            return;
        }
        if (key == "description" && tokens.size() == 2) {
            definition.description = tokens[1];
            return;
        }
        if (key == "damage" && tokens.size() == 2) {
            definition.damage = std::stof(tokens[1]);
            return;
        }
        if (key == "range" && tokens.size() == 2) {
            definition.range = std::stof(tokens[1]);
            return;
        }
        if (key == "cooldown" && tokens.size() == 2) {
            definition.cooldown = std::stof(tokens[1]);
            return;
        }
        throwParseError(path, lineNumber, "invalid weapon definition record");
    });
}

EnemyDefinition loadEnemyDefinitionAsset(const std::string& path) {
    return loadDefinitionFile<EnemyDefinition>(path, [&](EnemyDefinition& definition, const std::vector<std::string>& tokens, int lineNumber) {
        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            return;
        }
        if (key == "display_name" && tokens.size() == 2) {
            definition.displayName = tokens[1];
            return;
        }
        if (key == "mesh" && tokens.size() == 2) {
            definition.meshId = tokens[1];
            return;
        }
        if (key == "max_health" && tokens.size() == 2) {
            definition.maxHealth = std::stof(tokens[1]);
            return;
        }
        if (key == "move_speed" && tokens.size() == 2) {
            definition.moveSpeed = std::stof(tokens[1]);
            return;
        }
        if (key == "contact_damage" && tokens.size() == 2) {
            definition.contactDamage = std::stof(tokens[1]);
            return;
        }
        throwParseError(path, lineNumber, "invalid enemy definition record");
    });
}

ItemDefinition loadItemDefinitionAsset(const std::string& path) {
    return loadDefinitionFile<ItemDefinition>(path, [&](ItemDefinition& definition, const std::vector<std::string>& tokens, int lineNumber) {
        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            return;
        }
        if (key == "display_name" && tokens.size() == 2) {
            definition.displayName = tokens[1];
            return;
        }
        if (key == "mesh" && tokens.size() == 2) {
            definition.meshId = tokens[1];
            return;
        }
        if (key == "pickup_prompt" && tokens.size() == 2) {
            definition.pickupPrompt = tokens[1];
            return;
        }
        throwParseError(path, lineNumber, "invalid item definition record");
    });
}

SkillDefinition loadSkillDefinitionAsset(const std::string& path) {
    return loadDefinitionFile<SkillDefinition>(path, [&](SkillDefinition& definition, const std::vector<std::string>& tokens, int lineNumber) {
        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            return;
        }
        if (key == "display_name" && tokens.size() == 2) {
            definition.displayName = tokens[1];
            return;
        }
        if (key == "category" && tokens.size() == 2) {
            definition.category = tokens[1];
            return;
        }
        if (key == "description" && tokens.size() == 2) {
            definition.description = tokens[1];
            return;
        }
        if (key == "max_rank" && tokens.size() == 2) {
            definition.maxRank = std::stoi(tokens[1]);
            return;
        }
        throwParseError(path, lineNumber, "invalid skill definition record");
    });
}

GameplayArchetypeDefinition loadGameplayArchetypeAsset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open gameplay archetype asset: " + path);
    }

    GameplayArchetypeDefinition definition;
    bool hasType = false;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }

        const auto tokens = tokenizeRecord(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            continue;
        }
        if (key == "type" && tokens.size() == 2) {
            if (tokens[1] == "checkpoint") {
                definition.kind = GameplayArchetypeKind::Checkpoint;
            } else if (tokens[1] == "double_door") {
                definition.kind = GameplayArchetypeKind::DoubleDoor;
            } else {
                throwParseError(path, lineNumber, "unknown gameplay archetype type");
            }
            hasType = true;
            continue;
        }

        if (!hasType) {
            throwParseError(path, lineNumber, "archetype type must be declared before properties");
        }

        auto requireFloatCount = [&](std::size_t expected) {
            if (tokens.size() != expected) {
                throwParseError(path, lineNumber, "invalid gameplay archetype record");
            }
        };

        if (key == "position") {
            requireFloatCount(4);
            definition.checkpoint.position = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "respawn_position") {
            requireFloatCount(4);
            definition.checkpoint.respawnPosition = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "interact" && tokens.size() == 3) {
            if (definition.kind == GameplayArchetypeKind::Checkpoint) {
                definition.checkpoint.interactDistance = std::stof(tokens[1]);
                definition.checkpoint.interactDotThreshold = std::stof(tokens[2]);
            } else {
                definition.doubleDoor.interactDistance = std::stof(tokens[1]);
                definition.doubleDoor.interactDotThreshold = std::stof(tokens[2]);
            }
            continue;
        }
        if (key == "light_position") {
            requireFloatCount(4);
            definition.checkpoint.lightPosition = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "light_color") {
            requireFloatCount(4);
            definition.checkpoint.lightColor = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "light" && tokens.size() == 3) {
            definition.checkpoint.lightRadius = std::stof(tokens[1]);
            definition.checkpoint.lightIntensity = std::stof(tokens[2]);
            continue;
        }
        if (key == "left_leaf_mesh" && tokens.size() == 2) {
            definition.doubleDoor.leftLeafMeshName = tokens[1];
            continue;
        }
        if (key == "right_leaf_mesh" && tokens.size() == 2) {
            definition.doubleDoor.rightLeafMeshName = tokens[1];
            continue;
        }
        if (key == "root_position") {
            requireFloatCount(4);
            definition.doubleDoor.rootPosition = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "left_hinge_position") {
            requireFloatCount(4);
            definition.doubleDoor.leftHingePosition = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "right_hinge_position") {
            requireFloatCount(4);
            definition.doubleDoor.rightHingePosition = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "leaf_scale") {
            requireFloatCount(4);
            definition.doubleDoor.leafScale = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
            continue;
        }
        if (key == "closed_yaw" && tokens.size() == 2) {
            definition.doubleDoor.closedYaw = std::stof(tokens[1]);
            continue;
        }
        if (key == "open_angle" && tokens.size() == 2) {
            definition.doubleDoor.openAngle = std::stof(tokens[1]);
            continue;
        }
        if (key == "open_duration" && tokens.size() == 2) {
            definition.doubleDoor.openDuration = std::stof(tokens[1]);
            continue;
        }

        throwParseError(path, lineNumber, "unknown gameplay archetype key");
    }

    if (definition.id.empty()) {
        throw std::runtime_error("Gameplay archetype asset missing id: " + path);
    }
    if (!hasType) {
        throw std::runtime_error("Gameplay archetype asset missing type: " + path);
    }

    return definition;
}

GameplayPrefabInstance instantiateGameplayArchetype(const GameplayArchetypeDefinition& definition,
                                                    const glm::vec3& position,
                                                    float yawDegrees) {
    GameplayPrefabInstance instance;
    switch (definition.kind) {
    case GameplayArchetypeKind::Checkpoint:
        instance.type = GameplayPrefabType::Checkpoint;
        instance.checkpoint = definition.checkpoint;
        instance.checkpoint.position = transformPoint(definition.checkpoint.position, position, yawDegrees);
        instance.checkpoint.respawnPosition = transformPoint(definition.checkpoint.respawnPosition, position, yawDegrees);
        instance.checkpoint.lightPosition = transformPoint(definition.checkpoint.lightPosition, position, yawDegrees);
        break;
    case GameplayArchetypeKind::DoubleDoor:
        instance.type = GameplayPrefabType::DoubleDoor;
        instance.doubleDoor = definition.doubleDoor;
        instance.doubleDoor.rootPosition = transformPoint(definition.doubleDoor.rootPosition, position, yawDegrees);
        instance.doubleDoor.leftHingePosition = transformPoint(definition.doubleDoor.leftHingePosition, position, yawDegrees);
        instance.doubleDoor.rightHingePosition = transformPoint(definition.doubleDoor.rightHingePosition, position, yawDegrees);
        instance.doubleDoor.closedYaw += yawDegrees;
        break;
    }
    return instance;
}

void ContentRegistry::loadDefaults() {
    weapons_.clear();
    enemies_.clear();
    items_.clear();
    skills_.clear();
    archetypes_.clear();
    materials_.clear();

    auto oldDagger = loadWeaponDefinitionAsset(resolveProjectPath("assets/defs/weapons/old_dagger.weapon"));
    weapons_.emplace(oldDagger.id, oldDagger);

    auto brigandAxe = loadWeaponDefinitionAsset(resolveProjectPath("assets/defs/weapons/brigand_axe.weapon"));
    weapons_.emplace(brigandAxe.id, brigandAxe);

    auto enemy = loadEnemyDefinitionAsset(resolveProjectPath("assets/defs/enemies/sentinel.enemy"));
    enemies_.emplace(enemy.id, enemy);

    auto item = loadItemDefinitionAsset(resolveProjectPath("assets/defs/items/kindling_shard.item"));
    items_.emplace(item.id, item);

    auto skill = loadSkillDefinitionAsset(resolveProjectPath("assets/defs/skills/cathedral_attunement.skill"));
    skills_.emplace(skill.id, skill);

    auto checkpoint = loadGameplayArchetypeAsset(resolveProjectPath("assets/prefabs/gameplay/checkpoint.prefab"));
    archetypes_.emplace(checkpoint.id, checkpoint);

    auto doubleDoor = loadGameplayArchetypeAsset(resolveProjectPath("assets/prefabs/gameplay/double_door.prefab"));
    archetypes_.emplace(doubleDoor.id, doubleDoor);

    const std::array materialFiles{
        "assets/defs/materials/masonry_base.material",
        "assets/defs/materials/stone_default.material",
        "assets/defs/materials/wood_default.material",
        "assets/defs/materials/metal_default.material",
        "assets/defs/materials/wax_default.material",
        "assets/defs/materials/moss_default.material",
        "assets/defs/materials/floor_default.material",
        "assets/defs/materials/brick_default.material",
        "assets/defs/materials/viewmodel_default.material",
        "assets/defs/materials/brick_wall_old.material",
    };
    for (const char* path : materialFiles) {
        auto material = loadMaterialDefinitionAsset(resolveProjectPath(path));
        materials_.emplace(material.id, material);
    }
}

const WeaponDefinition* ContentRegistry::findWeapon(const std::string& id) const {
    auto it = weapons_.find(id);
    return it == weapons_.end() ? nullptr : &it->second;
}

const EnemyDefinition* ContentRegistry::findEnemy(const std::string& id) const {
    auto it = enemies_.find(id);
    return it == enemies_.end() ? nullptr : &it->second;
}

const ItemDefinition* ContentRegistry::findItem(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : &it->second;
}

const SkillDefinition* ContentRegistry::findSkill(const std::string& id) const {
    auto it = skills_.find(id);
    return it == skills_.end() ? nullptr : &it->second;
}

const GameplayArchetypeDefinition* ContentRegistry::findArchetype(const std::string& id) const {
    auto it = archetypes_.find(id);
    return it == archetypes_.end() ? nullptr : &it->second;
}

const MaterialDefinition* ContentRegistry::findMaterial(const std::string& id) const {
    auto it = materials_.find(id);
    return it == materials_.end() ? nullptr : &it->second;
}
