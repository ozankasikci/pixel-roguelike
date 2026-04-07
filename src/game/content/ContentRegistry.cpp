#include "game/content/ContentRegistry.h"
#include "engine/core/PathUtils.h"
#include "game/content/ParseUtils.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <spdlog/spdlog.h>

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
            definition.checkpoint.interactDistance = std::stof(tokens[1]);
            definition.checkpoint.interactDotThreshold = std::stof(tokens[2]);
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
    }

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

    loadMaterialsFromDirectory("assets/materials");
    validateMaterialInheritance();

    for (const auto& path : sortedDefinitionFiles("assets/defs/environments", ".environment")) {
        auto environment = loadEnvironmentDefinitionAsset(path);
        environmentPaths_.emplace(environment.id, path);
        environments_.emplace(environment.id, environment);
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

void ContentRegistry::loadMaterialsFromDirectory(const std::string& relativeDirectory) {
    namespace fs = std::filesystem;
    const fs::path directory = resolveProjectPath(relativeDirectory);
    if (!fs::exists(directory)) {
        spdlog::warn("Material directory does not exist: {}", directory.string());
        return;
    }
    for (const auto& entry : fs::recursive_directory_iterator(directory,
            fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".material") {
            continue;
        }
        try {
            auto material = loadMaterialDefinitionAsset(entry.path().string());
            if (materials_.count(material.id)) {
                spdlog::error("Duplicate material id '{}' in '{}' — skipped (first loaded from earlier scan)",
                              material.id, entry.path().string());
                continue;
            }
            materialFilePathById_[material.id] = entry.path().string();
            materialFileTimes_[entry.path().string()] = fs::last_write_time(entry.path());
            materials_.emplace(material.id, std::move(material));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load material '{}': {}", entry.path().string(), e.what());
        }
    }
}

void ContentRegistry::validateMaterialInheritance() {
    for (const auto& [id, def] : materials_) {
        if (def.parent.has_value() && !materials_.count(*def.parent)) {
            spdlog::error("Material '{}' has missing parent '{}' — will use magenta fallback",
                          id, *def.parent);
        }
        std::string error;
        if (!validateMaterialDefinition(def, error)) {
            spdlog::warn("Material '{}' validation: {}", id, error);
        }
    }
}

bool ContentRegistry::validateMaterialDefinition(const MaterialDefinition& def, std::string& errorOut) const {
    if (def.id.empty()) {
        errorOut = "Material ID is empty";
        return false;
    }
    if (def.parent.has_value() && !def.parent->empty()) {
        if (!materials_.count(*def.parent)) {
            errorOut = "Parent material '" + *def.parent + "' not found";
            return false;
        }
    }
    if (def.roughnessBias.has_value()) {
        float v = *def.roughnessBias;
        if (v < 0.0f || v > 1.0f) {
            errorOut = "roughness_bias " + std::to_string(v) + " out of range [0, 1]";
            return false;
        }
    }
    if (def.metalness.has_value()) {
        float v = *def.metalness;
        if (v < 0.0f || v > 1.0f) {
            errorOut = "metalness " + std::to_string(v) + " out of range [0, 1]";
            return false;
        }
    }
    return true;
}

void ContentRegistry::pollMaterialHotReload(MaterialTextureLibrary& texLibrary) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMaterialPoll_);
    if (elapsed.count() < kMaterialPollIntervalMs) return;
    lastMaterialPoll_ = now;

    namespace fs = std::filesystem;
    for (auto& [path, knownTime] : materialFileTimes_) {
        try {
            if (!fs::exists(path)) continue;
            auto currentTime = fs::last_write_time(path);
            if (currentTime == knownTime) continue;
            knownTime = currentTime;

            spdlog::info("Hot-reloading material: {}", path);
            auto updated = loadMaterialDefinitionAsset(path);

            std::string error;
            if (!validateMaterialDefinition(updated, error)) {
                spdlog::error("Hot-reload validation failed for '{}': {}", path, error);
                continue;
            }

            const std::string updatedId = updated.id;
            materials_[updatedId] = std::move(updated);

            texLibrary.reloadMaterial(updatedId, materials_);
        } catch (const std::exception& e) {
            spdlog::error("Hot-reload error for '{}': {}", path, e.what());
        }
    }
}

void ContentRegistry::addMaterial(MaterialDefinition def, const std::string& filePath) {
    namespace fs = std::filesystem;
    const std::string id = def.id;
    if (materials_.count(id)) {
        spdlog::warn("addMaterial: overwriting existing material '{}'", id);
    }
    materialFilePathById_[id] = filePath;
    if (fs::exists(filePath)) {
        materialFileTimes_[filePath] = fs::last_write_time(filePath);
    }
    materials_[id] = std::move(def);
    spdlog::info("Added material '{}' from '{}'", id, filePath);
}

void ContentRegistry::removeMaterial(const std::string& id) {
    auto pathIt = materialFilePathById_.find(id);
    if (pathIt != materialFilePathById_.end()) {
        materialFileTimes_.erase(pathIt->second);
        materialFilePathById_.erase(pathIt);
    }
    materials_.erase(id);
    spdlog::info("Removed material '{}'", id);
}
