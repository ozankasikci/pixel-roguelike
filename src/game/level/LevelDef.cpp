#include "game/level/LevelDef.h"

#include <cctype>
#include <fstream>
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

bool tryParseMaterialKind(const std::string& token, MaterialKind& material) {
    if (token == "stone") {
        material = MaterialKind::Stone;
        return true;
    }
    if (token == "wood") {
        material = MaterialKind::Wood;
        return true;
    }
    if (token == "metal") {
        material = MaterialKind::Metal;
        return true;
    }
    if (token == "wax") {
        material = MaterialKind::Wax;
        return true;
    }
    if (token == "moss") {
        material = MaterialKind::Moss;
        return true;
    }
    if (token == "floor") {
        material = MaterialKind::Floor;
        return true;
    }
    return false;
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
                std::string token;
                if (!(stream >> token)) {
                    throwParseError(path, lineNumber, "invalid mesh metadata");
                }

                MaterialKind material;
                if (tryParseMaterialKind(token, material)) {
                    placement.material = material;
                    stream >> std::ws;
                    if (!stream.eof()) {
                        glm::vec3 tint{1.0f};
                        if (!(stream >> tint.r >> tint.g >> tint.b)) {
                            throwParseError(path, lineNumber, "invalid mesh tint");
                        }
                        placement.tint = tint;
                    }
                } else {
                    glm::vec3 tint{1.0f};
                    if (!tryParseFloatToken(token, tint.r) || !(stream >> tint.g >> tint.b)) {
                        throwParseError(path, lineNumber, "invalid mesh tint");
                    }
                    placement.tint = tint;
                }
            }
            data.meshes.push_back(std::move(placement));
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
