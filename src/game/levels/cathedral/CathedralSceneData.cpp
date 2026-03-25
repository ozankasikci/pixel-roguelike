#include "game/levels/cathedral/CathedralSceneData.h"
#include "game/prefabs/GameplayPrefabAssets.h"

#include <cctype>
#include <filesystem>
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

std::string resolveRelativePath(const std::string& basePath, const std::string& relativePath) {
    const std::filesystem::path scenePath(basePath);
    return (scenePath.parent_path() / relativePath).lexically_normal().string();
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
            throwParseError(path, lineNumber, "inline prefab records are no longer supported; use prefab_instance files");
        }

        if (kind == "prefab_instance") {
            std::string prefabPath;
            glm::vec3 position(0.0f);
            float yawDegrees = 0.0f;
            if (!(stream >> prefabPath >> position.x >> position.y >> position.z >> yawDegrees)) {
                throwParseError(path, lineNumber, "invalid prefab_instance record");
            }
            const auto asset = loadGameplayPrefabAsset(resolveRelativePath(path, prefabPath));
            data.prefabs.push_back(instantiateGameplayPrefab(asset, position, yawDegrees));
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}
