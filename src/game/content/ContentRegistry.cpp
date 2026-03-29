#include "game/content/ContentRegistry.h"
#include "engine/core/PathUtils.h"
#include "game/scripting/ScriptManifest.h"

#include <cctype>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::string> sortedDefinitionFiles(const std::string& relativeDirectory,
                                               const std::string& extension) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    const fs::path directory = resolveProjectPath(relativeDirectory);
    if (!fs::exists(directory)) {
        return files;
    }
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != extension) {
            continue;
        }
        files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<std::string> sortedRecursiveFiles(const std::string& relativeDirectory,
                                              const std::string& extension) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    const fs::path directory = resolveProjectPath(relativeDirectory);
    if (!fs::exists(directory)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != extension) {
            continue;
        }
        files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

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
    while (stream >> std::quoted(token)) {
        tokens.push_back(token);
    }
    return tokens;
}

ScriptAttachment* findScriptAttachment(std::vector<ScriptAttachment>& attachments,
                                       const std::string& scriptId) {
    for (auto& attachment : attachments) {
        if (attachment.scriptId == scriptId) {
            return &attachment;
        }
    }
    return nullptr;
}

bool tryParseScriptValueRecord(const std::string& key,
                               ScriptPropertyType& type) {
    if (key == "script_number") {
        type = ScriptPropertyType::Number;
        return true;
    }
    if (key == "script_bool") {
        type = ScriptPropertyType::Boolean;
        return true;
    }
    if (key == "script_string") {
        type = ScriptPropertyType::String;
        return true;
    }
    if (key == "script_vec3") {
        type = ScriptPropertyType::Vec3;
        return true;
    }
    return false;
}

void appendScriptRecords(std::ostringstream& out,
                         const std::vector<ScriptAttachment>& attachments) {
    for (const auto& attachment : attachments) {
        out << "script_attach " << serializeScriptString(attachment.scriptId) << ' '
            << (attachment.enabled ? "true" : "false") << '\n';
        for (const auto& [propertyName, propertyValue] : attachment.propertyValues) {
            ScriptPropertyType type = ScriptPropertyType::Number;
            if (std::holds_alternative<double>(propertyValue)) {
                type = ScriptPropertyType::Number;
            } else if (std::holds_alternative<bool>(propertyValue)) {
                type = ScriptPropertyType::Boolean;
            } else if (std::holds_alternative<std::string>(propertyValue)) {
                type = ScriptPropertyType::String;
            } else if (std::holds_alternative<glm::vec3>(propertyValue)) {
                type = ScriptPropertyType::Vec3;
            }
            out << "script_" << scriptPropertyTypeName(type) << ' '
                << serializeScriptString(attachment.scriptId) << ' '
                << serializeScriptString(propertyName) << ' '
                << serializeScriptPropertyValue(propertyValue) << '\n';
        }
    }
}

std::string scriptIdForSourcePath(const std::filesystem::path& scriptPath) {
    namespace fs = std::filesystem;
    const fs::path scriptsRoot(resolveProjectPath("assets/scripts"));
    fs::path relative = fs::relative(scriptPath, scriptsRoot);
    relative.replace_extension();
    return relative.generic_string();
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
        if (key == "script_attach" && tokens.size() == 3) {
            if (findScriptAttachment(definition.scripts, tokens[1]) != nullptr) {
                throwParseError(path, lineNumber, "duplicate archetype script id");
            }
            ScriptAttachment attachment;
            attachment.scriptId = tokens[1];
            if (tokens[2] == "true") {
                attachment.enabled = true;
            } else if (tokens[2] == "false") {
                attachment.enabled = false;
            } else {
                throwParseError(path, lineNumber, "invalid archetype script enabled flag");
            }
            definition.scripts.push_back(std::move(attachment));
            continue;
        }

        ScriptPropertyType scriptType = ScriptPropertyType::Number;
        if (tryParseScriptValueRecord(key, scriptType)) {
            if (tokens.size() < 4) {
                throwParseError(path, lineNumber, "invalid archetype script property record");
            }
            auto* attachment = findScriptAttachment(definition.scripts, tokens[1]);
            if (attachment == nullptr) {
                throwParseError(path, lineNumber, "script property requires prior script_attach record");
            }
            ScriptPropertyValue propertyValue;
            std::size_t consumed = 0;
            if (!tryParseScriptPropertyValueTokens(scriptType, tokens, 3, propertyValue, consumed)
                || 3 + consumed != tokens.size()) {
                throwParseError(path, lineNumber, "invalid archetype script property value");
            }
            attachment->propertyValues[tokens[2]] = propertyValue;
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

std::string serializeGameplayArchetypeAsset(const GameplayArchetypeDefinition& definition) {
    if (definition.id.empty()) {
        throw std::runtime_error("Cannot serialize gameplay archetype without id");
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "id " << definition.id << '\n';
    switch (definition.kind) {
    case GameplayArchetypeKind::Checkpoint:
        out << "type checkpoint\n";
        out << "position "
            << definition.checkpoint.position.x << ' '
            << definition.checkpoint.position.y << ' '
            << definition.checkpoint.position.z << '\n';
        out << "respawn_position "
            << definition.checkpoint.respawnPosition.x << ' '
            << definition.checkpoint.respawnPosition.y << ' '
            << definition.checkpoint.respawnPosition.z << '\n';
        out << "interact "
            << definition.checkpoint.interactDistance << ' '
            << definition.checkpoint.interactDotThreshold << '\n';
        out << "light_position "
            << definition.checkpoint.lightPosition.x << ' '
            << definition.checkpoint.lightPosition.y << ' '
            << definition.checkpoint.lightPosition.z << '\n';
        out << "light_color "
            << definition.checkpoint.lightColor.x << ' '
            << definition.checkpoint.lightColor.y << ' '
            << definition.checkpoint.lightColor.z << '\n';
        out << "light "
            << definition.checkpoint.lightRadius << ' '
            << definition.checkpoint.lightIntensity << '\n';
        break;
    case GameplayArchetypeKind::DoubleDoor:
        out << "type double_door\n";
        out << "left_leaf_mesh " << definition.doubleDoor.leftLeafMeshName << '\n';
        out << "right_leaf_mesh " << definition.doubleDoor.rightLeafMeshName << '\n';
        out << "root_position "
            << definition.doubleDoor.rootPosition.x << ' '
            << definition.doubleDoor.rootPosition.y << ' '
            << definition.doubleDoor.rootPosition.z << '\n';
        out << "left_hinge_position "
            << definition.doubleDoor.leftHingePosition.x << ' '
            << definition.doubleDoor.leftHingePosition.y << ' '
            << definition.doubleDoor.leftHingePosition.z << '\n';
        out << "right_hinge_position "
            << definition.doubleDoor.rightHingePosition.x << ' '
            << definition.doubleDoor.rightHingePosition.y << ' '
            << definition.doubleDoor.rightHingePosition.z << '\n';
        out << "leaf_scale "
            << definition.doubleDoor.leafScale.x << ' '
            << definition.doubleDoor.leafScale.y << ' '
            << definition.doubleDoor.leafScale.z << '\n';
        out << "closed_yaw " << definition.doubleDoor.closedYaw << '\n';
        out << "open_angle " << definition.doubleDoor.openAngle << '\n';
        out << "interact "
            << definition.doubleDoor.interactDistance << ' '
            << definition.doubleDoor.interactDotThreshold << '\n';
        out << "open_duration " << definition.doubleDoor.openDuration << '\n';
        break;
    }
    appendScriptRecords(out, definition.scripts);
    return out.str();
}

void saveGameplayArchetypeAsset(const std::string& path, const GameplayArchetypeDefinition& definition) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to save gameplay archetype asset: " + path);
    }
    file << serializeGameplayArchetypeAsset(definition);
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
    environments_.clear();
    environmentPaths_.clear();
    scripts_.clear();

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
        "assets/defs/materials/cloister_stone.material",
    };
    for (const char* path : materialFiles) {
        auto material = loadMaterialDefinitionAsset(resolveProjectPath(path));
        materials_.emplace(material.id, material);
    }

    for (const auto& path : sortedDefinitionFiles("assets/defs/environments", ".environment")) {
        auto environment = loadEnvironmentDefinitionAsset(path);
        environmentPaths_.emplace(environment.id, path);
        environments_.emplace(environment.id, environment);
    }

    ScriptManifest manifest = loadScriptManifest(resolveProjectPath("build/generated/scripts/manifest.txt"));
    for (const auto& scriptFile : sortedRecursiveFiles("assets/scripts", ".ts")) {
        const std::filesystem::path scriptPath(scriptFile);
        const std::string scriptId = scriptIdForSourcePath(scriptPath);

        ScriptDefinition definition;
        if (const auto it = manifest.find(scriptId); it != manifest.end()) {
            definition = it->second;
        }
        definition.id = scriptId;
        definition.sourcePath = std::filesystem::relative(scriptPath, std::filesystem::current_path()).generic_string();
        if (definition.builtPath.empty()) {
            definition.builtPath = (std::filesystem::path("build/generated/scripts") / std::filesystem::path(scriptId + ".js")).generic_string();
        }

        const std::filesystem::path builtPath(resolveProjectPath(definition.builtPath));
        definition.stale = !std::filesystem::exists(builtPath)
            || std::filesystem::last_write_time(builtPath) < std::filesystem::last_write_time(scriptPath);
        scripts_.emplace(definition.id, std::move(definition));
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

const EnvironmentDefinition* ContentRegistry::findEnvironment(const std::string& id) const {
    auto it = environments_.find(id);
    return it == environments_.end() ? nullptr : &it->second;
}

const std::string* ContentRegistry::findEnvironmentPath(const std::string& id) const {
    auto it = environmentPaths_.find(id);
    return it == environmentPaths_.end() ? nullptr : &it->second;
}

const ScriptDefinition* ContentRegistry::findScript(const std::string& id) const {
    auto it = scripts_.find(id);
    return it == scripts_.end() ? nullptr : &it->second;
}
