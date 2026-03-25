#include "game/levels/cathedral/CathedralSceneData.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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

CathedralSceneData loadCathedralSceneData(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open cathedral scene data: " + path);
    }

    CathedralSceneData data;
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
            CathedralMeshPlacement placement;
            if (!(stream >> placement.meshName
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.scale.x >> placement.scale.y >> placement.scale.z
                         >> placement.rotation.x >> placement.rotation.y >> placement.rotation.z)) {
                throwParseError(path, lineNumber, "invalid mesh record");
            }
            data.meshes.push_back(std::move(placement));
            continue;
        }

        if (kind == "light") {
            CathedralLightPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.radius >> placement.intensity)) {
                throwParseError(path, lineNumber, "invalid light record");
            }
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "collider_box") {
            CathedralBoxColliderPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.halfExtents.x >> placement.halfExtents.y >> placement.halfExtents.z)) {
                throwParseError(path, lineNumber, "invalid collider_box record");
            }
            data.boxColliders.push_back(placement);
            continue;
        }

        if (kind == "collider_cylinder") {
            CathedralCylinderColliderPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.radius >> placement.halfHeight)) {
                throwParseError(path, lineNumber, "invalid collider_cylinder record");
            }
            data.cylinderColliders.push_back(placement);
            continue;
        }

        if (kind == "player_spawn") {
            CathedralPlayerSpawnPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.fallRespawnY)) {
                throwParseError(path, lineNumber, "invalid player_spawn record");
            }
            data.playerSpawn = placement;
            data.hasPlayerSpawn = true;
            continue;
        }

        if (kind == "checkpoint") {
            CathedralCheckpointPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.respawnPosition.x >> placement.respawnPosition.y >> placement.respawnPosition.z
                         >> placement.interactDistance >> placement.interactDotThreshold
                         >> placement.lightPosition.x >> placement.lightPosition.y >> placement.lightPosition.z
                         >> placement.lightColor.r >> placement.lightColor.g >> placement.lightColor.b
                         >> placement.lightRadius >> placement.lightIntensity)) {
                throwParseError(path, lineNumber, "invalid checkpoint record");
            }
            data.checkpoints.push_back(placement);
            continue;
        }

        if (kind == "double_door") {
            CathedralDoubleDoorPlacement placement;
            if (!(stream >> placement.rootPosition.x >> placement.rootPosition.y >> placement.rootPosition.z
                         >> placement.leftHingePosition.x >> placement.leftHingePosition.y >> placement.leftHingePosition.z
                         >> placement.rightHingePosition.x >> placement.rightHingePosition.y >> placement.rightHingePosition.z
                         >> placement.leafScale.x >> placement.leafScale.y >> placement.leafScale.z
                         >> placement.closedYaw >> placement.openAngle
                         >> placement.interactDistance >> placement.interactDotThreshold >> placement.openDuration)) {
                throwParseError(path, lineNumber, "invalid double_door record");
            }
            data.doors.push_back(placement);
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}
