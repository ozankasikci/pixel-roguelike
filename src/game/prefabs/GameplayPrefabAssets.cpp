#include "game/prefabs/GameplayPrefabAssets.h"

#include <cctype>
#include <cmath>
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

GameplayPrefabAsset loadGameplayPrefabAsset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open gameplay prefab asset: " + path);
    }

    GameplayPrefabAsset asset;
    bool hasType = false;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }

        std::istringstream stream(line);
        std::string key;
        stream >> key;

        if (key == "type") {
            std::string typeName;
            if (!(stream >> typeName)) {
                throwParseError(path, lineNumber, "missing prefab type");
            }
            if (typeName == "checkpoint") {
                asset.type = GameplayPrefabType::Checkpoint;
            } else if (typeName == "double_door") {
                asset.type = GameplayPrefabType::DoubleDoor;
            } else {
                throwParseError(path, lineNumber, "unknown prefab type '" + typeName + "'");
            }
            hasType = true;
            continue;
        }

        if (!hasType) {
            throwParseError(path, lineNumber, "prefab type must be declared before properties");
        }

        if (key == "position") {
            if (!(stream >> asset.checkpoint.position.x >> asset.checkpoint.position.y >> asset.checkpoint.position.z)) {
                throwParseError(path, lineNumber, "invalid position record");
            }
            continue;
        }

        if (key == "respawn_position") {
            if (!(stream >> asset.checkpoint.respawnPosition.x >> asset.checkpoint.respawnPosition.y >> asset.checkpoint.respawnPosition.z)) {
                throwParseError(path, lineNumber, "invalid respawn_position record");
            }
            continue;
        }

        if (key == "interact") {
            if (asset.type == GameplayPrefabType::Checkpoint) {
                if (!(stream >> asset.checkpoint.interactDistance >> asset.checkpoint.interactDotThreshold)) {
                    throwParseError(path, lineNumber, "invalid checkpoint interact record");
                }
            } else {
                if (!(stream >> asset.doubleDoor.interactDistance >> asset.doubleDoor.interactDotThreshold)) {
                    throwParseError(path, lineNumber, "invalid double_door interact record");
                }
            }
            continue;
        }

        if (key == "light_position") {
            if (!(stream >> asset.checkpoint.lightPosition.x >> asset.checkpoint.lightPosition.y >> asset.checkpoint.lightPosition.z)) {
                throwParseError(path, lineNumber, "invalid light_position record");
            }
            continue;
        }

        if (key == "light_color") {
            if (!(stream >> asset.checkpoint.lightColor.r >> asset.checkpoint.lightColor.g >> asset.checkpoint.lightColor.b)) {
                throwParseError(path, lineNumber, "invalid light_color record");
            }
            continue;
        }

        if (key == "light") {
            if (!(stream >> asset.checkpoint.lightRadius >> asset.checkpoint.lightIntensity)) {
                throwParseError(path, lineNumber, "invalid light record");
            }
            continue;
        }

        if (key == "left_leaf_mesh") {
            if (!(stream >> asset.doubleDoor.leftLeafMeshName)) {
                throwParseError(path, lineNumber, "invalid left_leaf_mesh record");
            }
            continue;
        }

        if (key == "right_leaf_mesh") {
            if (!(stream >> asset.doubleDoor.rightLeafMeshName)) {
                throwParseError(path, lineNumber, "invalid right_leaf_mesh record");
            }
            continue;
        }

        if (key == "root_position") {
            if (!(stream >> asset.doubleDoor.rootPosition.x >> asset.doubleDoor.rootPosition.y >> asset.doubleDoor.rootPosition.z)) {
                throwParseError(path, lineNumber, "invalid root_position record");
            }
            continue;
        }

        if (key == "left_hinge_position") {
            if (!(stream >> asset.doubleDoor.leftHingePosition.x >> asset.doubleDoor.leftHingePosition.y >> asset.doubleDoor.leftHingePosition.z)) {
                throwParseError(path, lineNumber, "invalid left_hinge_position record");
            }
            continue;
        }

        if (key == "right_hinge_position") {
            if (!(stream >> asset.doubleDoor.rightHingePosition.x >> asset.doubleDoor.rightHingePosition.y >> asset.doubleDoor.rightHingePosition.z)) {
                throwParseError(path, lineNumber, "invalid right_hinge_position record");
            }
            continue;
        }

        if (key == "leaf_scale") {
            if (!(stream >> asset.doubleDoor.leafScale.x >> asset.doubleDoor.leafScale.y >> asset.doubleDoor.leafScale.z)) {
                throwParseError(path, lineNumber, "invalid leaf_scale record");
            }
            continue;
        }

        if (key == "closed_yaw") {
            if (!(stream >> asset.doubleDoor.closedYaw)) {
                throwParseError(path, lineNumber, "invalid closed_yaw record");
            }
            continue;
        }

        if (key == "open_angle") {
            if (!(stream >> asset.doubleDoor.openAngle)) {
                throwParseError(path, lineNumber, "invalid open_angle record");
            }
            continue;
        }

        if (key == "open_duration") {
            if (!(stream >> asset.doubleDoor.openDuration)) {
                throwParseError(path, lineNumber, "invalid open_duration record");
            }
            continue;
        }

        throwParseError(path, lineNumber, "unknown prefab asset key '" + key + "'");
    }

    if (!hasType) {
        throw std::runtime_error("Gameplay prefab asset missing type: " + path);
    }

    return asset;
}

GameplayPrefabInstance instantiateGameplayPrefab(const GameplayPrefabAsset& asset,
                                                 const glm::vec3& position,
                                                 float yawDegrees) {
    GameplayPrefabInstance instance;
    instance.type = asset.type;

    switch (asset.type) {
    case GameplayPrefabType::Checkpoint:
        instance.checkpoint = asset.checkpoint;
        instance.checkpoint.position = transformPoint(asset.checkpoint.position, position, yawDegrees);
        instance.checkpoint.respawnPosition = transformPoint(asset.checkpoint.respawnPosition, position, yawDegrees);
        instance.checkpoint.lightPosition = transformPoint(asset.checkpoint.lightPosition, position, yawDegrees);
        return instance;
    case GameplayPrefabType::DoubleDoor:
        instance.doubleDoor = asset.doubleDoor;
        instance.doubleDoor.rootPosition = transformPoint(asset.doubleDoor.rootPosition, position, yawDegrees);
        instance.doubleDoor.leftHingePosition = transformPoint(asset.doubleDoor.leftHingePosition, position, yawDegrees);
        instance.doubleDoor.rightHingePosition = transformPoint(asset.doubleDoor.rightHingePosition, position, yawDegrees);
        instance.doubleDoor.closedYaw += yawDegrees;
        return instance;
    }

    return instance;
}
