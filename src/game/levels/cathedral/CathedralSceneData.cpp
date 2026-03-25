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

GameplayPrefabInstance parseCheckpointPrefab(std::istringstream& stream,
                                             const std::string& path,
                                             int lineNumber) {
    GameplayPrefabInstance instance;
    instance.type = GameplayPrefabType::Checkpoint;
    if (!(stream >> instance.checkpoint.position.x >> instance.checkpoint.position.y >> instance.checkpoint.position.z
                 >> instance.checkpoint.respawnPosition.x >> instance.checkpoint.respawnPosition.y >> instance.checkpoint.respawnPosition.z
                 >> instance.checkpoint.interactDistance >> instance.checkpoint.interactDotThreshold
                 >> instance.checkpoint.lightPosition.x >> instance.checkpoint.lightPosition.y >> instance.checkpoint.lightPosition.z
                 >> instance.checkpoint.lightColor.r >> instance.checkpoint.lightColor.g >> instance.checkpoint.lightColor.b
                 >> instance.checkpoint.lightRadius >> instance.checkpoint.lightIntensity)) {
        throwParseError(path, lineNumber, "invalid prefab checkpoint record");
    }
    return instance;
}

GameplayPrefabInstance parseDoubleDoorPrefab(std::istringstream& stream,
                                             const std::string& path,
                                             int lineNumber) {
    GameplayPrefabInstance instance;
    instance.type = GameplayPrefabType::DoubleDoor;
    if (!(stream >> instance.doubleDoor.rootPosition.x >> instance.doubleDoor.rootPosition.y >> instance.doubleDoor.rootPosition.z
                 >> instance.doubleDoor.leftHingePosition.x >> instance.doubleDoor.leftHingePosition.y >> instance.doubleDoor.leftHingePosition.z
                 >> instance.doubleDoor.rightHingePosition.x >> instance.doubleDoor.rightHingePosition.y >> instance.doubleDoor.rightHingePosition.z
                 >> instance.doubleDoor.leafScale.x >> instance.doubleDoor.leafScale.y >> instance.doubleDoor.leafScale.z
                 >> instance.doubleDoor.closedYaw >> instance.doubleDoor.openAngle
                 >> instance.doubleDoor.interactDistance >> instance.doubleDoor.interactDotThreshold >> instance.doubleDoor.openDuration)) {
        throwParseError(path, lineNumber, "invalid prefab double_door record");
    }
    return instance;
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

        if (kind == "prefab") {
            std::string prefabType;
            stream >> prefabType;
            if (prefabType == "checkpoint") {
                data.prefabs.push_back(parseCheckpointPrefab(stream, path, lineNumber));
                continue;
            }
            if (prefabType == "double_door") {
                data.prefabs.push_back(parseDoubleDoorPrefab(stream, path, lineNumber));
                continue;
            }
            throwParseError(path, lineNumber, "unknown prefab type '" + prefabType + "'");
        }

        if (kind == "checkpoint") {
            data.prefabs.push_back(parseCheckpointPrefab(stream, path, lineNumber));
            continue;
        }

        if (kind == "double_door") {
            data.prefabs.push_back(parseDoubleDoorPrefab(stream, path, lineNumber));
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}
