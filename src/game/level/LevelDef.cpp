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
