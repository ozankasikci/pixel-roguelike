#include "game/level/LevelDef.h"

#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MaterialDefinition.h"

#include <filesystem>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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

bool tryParseFloatToken(const std::string& token, float& value) {
    std::size_t parsed = 0;
    try {
        value = std::stof(token, &parsed);
    } catch (const std::exception&) {
        return false;
    }
    return parsed == token.size();
}

bool tryParseBoolToken(const std::string& token, bool& value) {
    if (token == "1" || token == "true" || token == "TRUE") {
        value = true;
        return true;
    }
    if (token == "0" || token == "false" || token == "FALSE") {
        value = false;
        return true;
    }
    return false;
}

std::string formatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
    return text;
}

std::string resolvedEnvironmentId(const LevelDef& data) {
    if (!data.environmentId.empty()) {
        return data.environmentId;
    }
    return environmentProfileName(data.environmentProfile);
}

std::string resolvedMaterialId(const LevelMeshPlacement& placement) {
    if (!placement.materialId.empty()) {
        return placement.materialId;
    }
    return std::string(defaultMaterialIdForKind(placement.material.value_or(MaterialKind::Stone)));
}

} // namespace

LevelDef loadLevelDef(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open level definition: " + path);
    }

    LevelDef data;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }

        std::istringstream stream(line);
        std::string kind;
        stream >> kind;

        if (kind == "mesh") {
            LevelMeshPlacement placement;
            if (!(stream >> placement.meshId
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.scale.x >> placement.scale.y >> placement.scale.z
                         >> placement.rotation.x >> placement.rotation.y >> placement.rotation.z)) {
                throwParseError(path, lineNumber, "invalid mesh record");
            }
            stream >> std::ws;
            if (!stream.eof()) {
                std::vector<std::string> tokens;
                std::string token;
                while (stream >> token) {
                    tokens.push_back(token);
                }

                if (!tokens.empty()) {
                    std::size_t index = 0;

                    MaterialKind legacyMaterial;
                    if (tryParseMaterialKindToken(tokens[0], legacyMaterial)) {
                        placement.material = legacyMaterial;
                        placement.materialId = std::string(defaultMaterialIdForKind(legacyMaterial));
                        index = 1;
                        if (index < tokens.size() && tokens[index] == "tint") {
                            ++index;
                        }
                        if (index < tokens.size()) {
                            if (index + 2 >= tokens.size()) {
                                throwParseError(path, lineNumber, "invalid mesh tint");
                            }
                            glm::vec3 tint{1.0f};
                            if (!tryParseFloatToken(tokens[index], tint.r)
                                || !tryParseFloatToken(tokens[index + 1], tint.g)
                                || !tryParseFloatToken(tokens[index + 2], tint.b)) {
                                throwParseError(path, lineNumber, "invalid mesh tint");
                            }
                            placement.tint = tint;
                            index += 3;
                        }
                        if (index != tokens.size()) {
                            throwParseError(path, lineNumber, "invalid mesh metadata");
                        }
                    } else if (tokens[0] == "material" || tokens[0] == "tint") {
                        while (index < tokens.size()) {
                            if (tokens[index] == "material") {
                                if (index + 1 >= tokens.size()) {
                                    throwParseError(path, lineNumber, "missing material id");
                                }
                                placement.materialId = tokens[index + 1];
                                index += 2;
                                continue;
                            }
                            if (tokens[index] == "tint") {
                                if (index + 3 >= tokens.size()) {
                                    throwParseError(path, lineNumber, "invalid mesh tint");
                                }
                                glm::vec3 tint{1.0f};
                                if (!tryParseFloatToken(tokens[index + 1], tint.r)
                                    || !tryParseFloatToken(tokens[index + 2], tint.g)
                                    || !tryParseFloatToken(tokens[index + 3], tint.b)) {
                                    throwParseError(path, lineNumber, "invalid mesh tint");
                                }
                                placement.tint = tint;
                                index += 4;
                                continue;
                            }
                            throwParseError(path, lineNumber, "invalid mesh metadata");
                        }
                    } else {
                        glm::vec3 tint{1.0f};
                        if (tokens.size() != 3
                            || !tryParseFloatToken(tokens[0], tint.r)
                            || !tryParseFloatToken(tokens[1], tint.g)
                            || !tryParseFloatToken(tokens[2], tint.b)) {
                            throwParseError(path, lineNumber, "invalid mesh metadata");
                        }
                        placement.tint = tint;
                    }
                }
            }
            data.meshes.push_back(std::move(placement));
            continue;
        }

        if (kind == "environment_profile") {
            std::string token;
            if (!(stream >> token)) {
                throwParseError(path, lineNumber, "invalid environment_profile record");
            }
            data.environmentId = token;
            if (tryParseEnvironmentProfileToken(token, data.environmentProfile)) {
                continue;
            }
            continue;
        }

        if (kind == "light") {
            LevelLightPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.radius >> placement.intensity)) {
                throwParseError(path, lineNumber, "invalid light record");
            }
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "spot_light") {
            LevelLightPlacement placement;
            placement.type = LightType::Spot;
            std::string shadowToken;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.direction.x >> placement.direction.y >> placement.direction.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.radius >> placement.intensity
                         >> placement.innerConeDegrees >> placement.outerConeDegrees
                         >> shadowToken)) {
                throwParseError(path, lineNumber, "invalid spot_light record");
            }
            if (!tryParseBoolToken(shadowToken, placement.castsShadows)) {
                throwParseError(path, lineNumber, "invalid spot_light shadow flag");
            }
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "dir_light") {
            LevelLightPlacement placement;
            placement.type = LightType::Directional;
            if (!(stream >> placement.direction.x >> placement.direction.y >> placement.direction.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.intensity)) {
                throwParseError(path, lineNumber, "invalid dir_light record");
            }
            placement.radius = 0.0f;
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "collider_box") {
            LevelBoxColliderPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.halfExtents.x >> placement.halfExtents.y >> placement.halfExtents.z)) {
                throwParseError(path, lineNumber, "invalid collider_box record");
            }
            data.boxColliders.push_back(placement);
            continue;
        }

        if (kind == "collider_cylinder") {
            LevelCylinderColliderPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.radius >> placement.halfHeight)) {
                throwParseError(path, lineNumber, "invalid collider_cylinder record");
            }
            data.cylinderColliders.push_back(placement);
            continue;
        }

        if (kind == "player_spawn") {
            LevelPlayerSpawn placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.fallRespawnY)) {
                throwParseError(path, lineNumber, "invalid player_spawn record");
            }
            data.playerSpawn = placement;
            data.hasPlayerSpawn = true;
            continue;
        }

        if (kind == "archetype_instance") {
            LevelArchetypePlacement placement;
            if (!(stream >> placement.archetypeId
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.yawDegrees)) {
                throwParseError(path, lineNumber, "invalid archetype_instance record");
            }
            data.archetypes.push_back(std::move(placement));
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}

std::string serializeLevelDef(const LevelDef& data) {
    std::ostringstream out;
    out << "environment_profile " << resolvedEnvironmentId(data) << "\n\n";

    for (const auto& placement : data.meshes) {
        out << "mesh " << placement.meshId << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.scale.x) << ' '
            << formatFloat(placement.scale.y) << ' '
            << formatFloat(placement.scale.z) << ' '
            << formatFloat(placement.rotation.x) << ' '
            << formatFloat(placement.rotation.y) << ' '
            << formatFloat(placement.rotation.z) << ' '
            << "material " << resolvedMaterialId(placement);
        if (placement.tint.has_value()) {
            out << " tint "
                << formatFloat(placement.tint->r) << ' '
                << formatFloat(placement.tint->g) << ' '
                << formatFloat(placement.tint->b);
        }
        out << '\n';
    }

    for (const auto& placement : data.lights) {
        if (placement.type == LightType::Spot) {
            out << "spot_light "
                << formatFloat(placement.position.x) << ' '
                << formatFloat(placement.position.y) << ' '
                << formatFloat(placement.position.z) << ' '
                << formatFloat(placement.direction.x) << ' '
                << formatFloat(placement.direction.y) << ' '
                << formatFloat(placement.direction.z) << ' '
                << formatFloat(placement.color.r) << ' '
                << formatFloat(placement.color.g) << ' '
                << formatFloat(placement.color.b) << ' '
                << formatFloat(placement.radius) << ' '
                << formatFloat(placement.intensity) << ' '
                << formatFloat(placement.innerConeDegrees) << ' '
                << formatFloat(placement.outerConeDegrees) << ' '
                << (placement.castsShadows ? "true" : "false") << '\n';
            continue;
        }

        if (placement.type == LightType::Directional) {
            out << "dir_light "
                << formatFloat(placement.direction.x) << ' '
                << formatFloat(placement.direction.y) << ' '
                << formatFloat(placement.direction.z) << ' '
                << formatFloat(placement.color.r) << ' '
                << formatFloat(placement.color.g) << ' '
                << formatFloat(placement.color.b) << ' '
                << formatFloat(placement.intensity) << '\n';
            continue;
        }

        out << "light "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.color.r) << ' '
            << formatFloat(placement.color.g) << ' '
            << formatFloat(placement.color.b) << ' '
            << formatFloat(placement.radius) << ' '
            << formatFloat(placement.intensity) << '\n';
    }

    for (const auto& placement : data.boxColliders) {
        out << "collider_box "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.halfExtents.x) << ' '
            << formatFloat(placement.halfExtents.y) << ' '
            << formatFloat(placement.halfExtents.z) << '\n';
    }

    for (const auto& placement : data.cylinderColliders) {
        out << "collider_cylinder "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.radius) << ' '
            << formatFloat(placement.halfHeight) << '\n';
    }

    if (data.hasPlayerSpawn) {
        out << "player_spawn "
            << formatFloat(data.playerSpawn.position.x) << ' '
            << formatFloat(data.playerSpawn.position.y) << ' '
            << formatFloat(data.playerSpawn.position.z) << ' '
            << formatFloat(data.playerSpawn.fallRespawnY) << '\n';
    }

    for (const auto& placement : data.archetypes) {
        out << "archetype_instance " << placement.archetypeId << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.yawDegrees) << '\n';
    }

    return out.str();
}

void saveLevelDef(const std::string& path, const LevelDef& data) {
    namespace fs = std::filesystem;
    const fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path());
    }
    std::ofstream out(target);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to write level definition: " + target.string());
    }
    out << serializeLevelDef(data);
}
