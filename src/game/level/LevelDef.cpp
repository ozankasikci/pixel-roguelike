#include "game/level/LevelDef.h"

#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MaterialDefinition.h"

#include <filesystem>
#include <cctype>
#include <functional>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <unordered_map>
#include <unordered_set>

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

std::vector<std::string> collectRemainingTokens(std::istream& stream) {
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool tryParseVec3Tokens(const std::vector<std::string>& tokens,
                        std::size_t index,
                        glm::vec3& value) {
    if (index + 2 >= tokens.size()) {
        return false;
    }
    return tryParseFloatToken(tokens[index], value.x)
        && tryParseFloatToken(tokens[index + 1], value.y)
        && tryParseFloatToken(tokens[index + 2], value.z);
}

void appendNodeMetadata(std::ostringstream& out,
                        const std::string& nodeId,
                        const std::string& parentNodeId) {
    if (!nodeId.empty()) {
        out << " node " << nodeId;
    }
    if (!parentNodeId.empty()) {
        out << " parent " << parentNodeId;
    }
}

glm::mat4 makeTransformMatrix(const glm::vec3& position,
                              const glm::vec3& rotationDegrees,
                              const glm::vec3& scale) {
    glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, scale);
    return matrix;
}

glm::mat3 extractRotationMatrix(const glm::mat4& matrix) {
    glm::mat3 basis(matrix);
    for (int column = 0; column < 3; ++column) {
        const float length = glm::length(basis[column]);
        if (length > 0.0001f) {
            basis[column] /= length;
        }
    }
    return basis;
}

bool decomposeTransformMatrix(const glm::mat4& matrix,
                              glm::vec3& position,
                              glm::vec3& rotationDegrees,
                              glm::vec3& scale) {
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat orientation;
    if (!glm::decompose(matrix, scale, orientation, position, skew, perspective)) {
        return false;
    }
    rotationDegrees = glm::degrees(glm::eulerAngles(orientation));
    return true;
}

bool hasNonZeroVec3(const glm::vec3& value) {
    return std::abs(value.x) > 0.0001f
        || std::abs(value.y) > 0.0001f
        || std::abs(value.z) > 0.0001f;
}

template <typename PlacementT>
const std::string& placementNodeId(const PlacementT& placement) {
    return placement.nodeId;
}

template <typename PlacementT>
const std::string& placementParentNodeId(const PlacementT& placement) {
    return placement.parentNodeId;
}

struct LevelNodeRef {
    enum class Kind {
        Mesh,
        Light,
        BoxCollider,
        CylinderCollider,
        PlayerSpawn,
        Archetype,
    };

    Kind kind = Kind::Mesh;
    std::size_t index = 0;
    std::string nodeId;
    std::string parentNodeId;
};

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
                const auto tokens = collectRemainingTokens(stream);

                if (!tokens.empty()) {
                    std::size_t index = 0;
                    while (index < tokens.size()) {
                        MaterialKind legacyMaterial;
                        glm::vec3 tint{1.0f};

                        if (index == 0 && tryParseMaterialKindToken(tokens[index], legacyMaterial)) {
                            placement.material = legacyMaterial;
                            placement.materialId = std::string(defaultMaterialIdForKind(legacyMaterial));
                            ++index;
                            continue;
                        }
                        if (tokens[index] == "material") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing material id");
                            }
                            placement.materialId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tokens[index] == "tint") {
                            if (!tryParseVec3Tokens(tokens, index + 1, tint)) {
                                throwParseError(path, lineNumber, "invalid mesh tint");
                            }
                            placement.tint = tint;
                            index += 4;
                            continue;
                        }
                        if (tokens[index] == "node") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing mesh node id");
                            }
                            placement.nodeId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tokens[index] == "parent") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing mesh parent node id");
                            }
                            placement.parentNodeId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tryParseVec3Tokens(tokens, index, tint)) {
                            placement.tint = tint;
                            index += 3;
                            continue;
                        }
                        throwParseError(path, lineNumber, "invalid mesh metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid light metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing spot_light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing spot_light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid spot_light metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing dir_light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing dir_light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid dir_light metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "rotation") {
                    if (!tryParseVec3Tokens(tokens, index + 1, placement.rotation)) {
                        throwParseError(path, lineNumber, "invalid collider_box rotation");
                    }
                    index += 4;
                    continue;
                }
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_box node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_box parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid collider_box metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "rotation") {
                    if (!tryParseVec3Tokens(tokens, index + 1, placement.rotation)) {
                        throwParseError(path, lineNumber, "invalid collider_cylinder rotation");
                    }
                    index += 4;
                    continue;
                }
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_cylinder node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_cylinder parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid collider_cylinder metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing player_spawn node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing player_spawn parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid player_spawn metadata");
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
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing archetype node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing archetype parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid archetype metadata");
            }
            data.archetypes.push_back(std::move(placement));
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}

LevelDef resolveLevelHierarchy(const LevelDef& data) {
    LevelDef resolved = data;

    std::vector<LevelNodeRef> refs;
    refs.reserve(data.meshes.size()
                 + data.lights.size()
                 + data.boxColliders.size()
                 + data.cylinderColliders.size()
                 + data.archetypes.size()
                 + (data.hasPlayerSpawn ? 1u : 0u));

    for (std::size_t i = 0; i < data.meshes.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Mesh, i, data.meshes[i].nodeId, data.meshes[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.lights.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Light, i, data.lights[i].nodeId, data.lights[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.boxColliders.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::BoxCollider, i, data.boxColliders[i].nodeId, data.boxColliders[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.cylinderColliders.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::CylinderCollider, i, data.cylinderColliders[i].nodeId, data.cylinderColliders[i].parentNodeId});
    }
    if (data.hasPlayerSpawn) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::PlayerSpawn, 0, data.playerSpawn.nodeId, data.playerSpawn.parentNodeId});
    }
    for (std::size_t i = 0; i < data.archetypes.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Archetype, i, data.archetypes[i].nodeId, data.archetypes[i].parentNodeId});
    }

    std::unordered_map<std::string, std::size_t> nodeLookup;
    for (std::size_t i = 0; i < refs.size(); ++i) {
        if (refs[i].nodeId.empty()) {
            continue;
        }
        const auto [it, inserted] = nodeLookup.emplace(refs[i].nodeId, i);
        if (!inserted) {
            throw std::runtime_error("Duplicate level node id: " + refs[i].nodeId);
        }
    }

    auto localMatrixFor = [&](const LevelNodeRef& ref) {
        switch (ref.kind) {
        case LevelNodeRef::Kind::Mesh: {
            const auto& placement = data.meshes[ref.index];
            return makeTransformMatrix(placement.position, placement.rotation, placement.scale);
        }
        case LevelNodeRef::Kind::Light: {
            const auto& placement = data.lights[ref.index];
            if (placement.type == LightType::Directional) {
                return glm::mat4(1.0f);
            }
            return makeTransformMatrix(placement.position, glm::vec3(0.0f), glm::vec3(1.0f));
        }
        case LevelNodeRef::Kind::BoxCollider: {
            const auto& placement = data.boxColliders[ref.index];
            return makeTransformMatrix(placement.position, placement.rotation, placement.halfExtents * 2.0f);
        }
        case LevelNodeRef::Kind::CylinderCollider: {
            const auto& placement = data.cylinderColliders[ref.index];
            return makeTransformMatrix(placement.position,
                                       placement.rotation,
                                       glm::vec3(placement.radius * 2.0f, placement.halfHeight * 2.0f, placement.radius * 2.0f));
        }
        case LevelNodeRef::Kind::PlayerSpawn:
            return makeTransformMatrix(data.playerSpawn.position, glm::vec3(0.0f), glm::vec3(1.0f));
        case LevelNodeRef::Kind::Archetype: {
            const auto& placement = data.archetypes[ref.index];
            return makeTransformMatrix(placement.position, glm::vec3(0.0f, placement.yawDegrees, 0.0f), glm::vec3(1.0f));
        }
        }
        return glm::mat4(1.0f);
    };

    std::vector<glm::mat4> worldMatrices(refs.size(), glm::mat4(1.0f));
    std::vector<glm::mat3> worldRotations(refs.size(), glm::mat3(1.0f));
    std::vector<bool> resolvedFlags(refs.size(), false);
    std::vector<bool> visiting(refs.size(), false);

    auto safeNormalize = [](const glm::vec3& value, const glm::vec3& fallback) {
        if (glm::dot(value, value) <= 0.0001f) {
            return fallback;
        }
        return glm::normalize(value);
    };

    std::function<void(std::size_t)> resolveRef = [&](std::size_t idx) {
        if (resolvedFlags[idx]) {
            return;
        }
        if (visiting[idx]) {
            throw std::runtime_error("Cycle detected in level hierarchy");
        }
        visiting[idx] = true;

        glm::mat4 parentMatrix(1.0f);
        glm::mat3 parentRotation(1.0f);
        const std::string& parentNodeId = refs[idx].parentNodeId;
        if (!parentNodeId.empty()) {
            const auto parentIt = nodeLookup.find(parentNodeId);
            if (parentIt != nodeLookup.end()) {
                resolveRef(parentIt->second);
                parentMatrix = worldMatrices[parentIt->second];
                parentRotation = worldRotations[parentIt->second];
            }
        }

        worldMatrices[idx] = parentMatrix * localMatrixFor(refs[idx]);
        worldRotations[idx] = extractRotationMatrix(worldMatrices[idx]);

        switch (refs[idx].kind) {
        case LevelNodeRef::Kind::Mesh: {
            auto& placement = resolved.meshes[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.scale = scale;
            }
            break;
        }
        case LevelNodeRef::Kind::Light: {
            auto& placement = resolved.lights[refs[idx].index];
            if (placement.type != LightType::Directional) {
                placement.position = glm::vec3(worldMatrices[idx][3]);
            }
            placement.direction = safeNormalize(parentRotation * data.lights[refs[idx].index].direction,
                                               placement.type == LightType::Directional
                                                   ? glm::vec3(0.0f, -1.0f, 0.0f)
                                                   : glm::vec3(0.0f, 0.0f, -1.0f));
            break;
        }
        case LevelNodeRef::Kind::BoxCollider: {
            auto& placement = resolved.boxColliders[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.halfExtents = glm::max(scale * 0.5f, glm::vec3(0.001f));
            }
            break;
        }
        case LevelNodeRef::Kind::CylinderCollider: {
            auto& placement = resolved.cylinderColliders[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.radius = std::max(0.001f, (std::abs(scale.x) + std::abs(scale.z)) * 0.25f);
                placement.halfHeight = std::max(0.001f, std::abs(scale.y) * 0.5f);
            }
            break;
        }
        case LevelNodeRef::Kind::PlayerSpawn:
            resolved.playerSpawn.position = glm::vec3(worldMatrices[idx][3]);
            break;
        case LevelNodeRef::Kind::Archetype: {
            auto& placement = resolved.archetypes[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.yawDegrees = rotation.y;
            }
            break;
        }
        }

        visiting[idx] = false;
        resolvedFlags[idx] = true;
    };

    for (std::size_t i = 0; i < refs.size(); ++i) {
        resolveRef(i);
    }

    return resolved;
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
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
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
                << (placement.castsShadows ? "true" : "false");
            appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
            out << '\n';
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
                << formatFloat(placement.intensity);
            appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
            out << '\n';
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
            << formatFloat(placement.intensity);
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    for (const auto& placement : data.boxColliders) {
        out << "collider_box "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.halfExtents.x) << ' '
            << formatFloat(placement.halfExtents.y) << ' '
            << formatFloat(placement.halfExtents.z);
        if (hasNonZeroVec3(placement.rotation)) {
            out << " rotation "
                << formatFloat(placement.rotation.x) << ' '
                << formatFloat(placement.rotation.y) << ' '
                << formatFloat(placement.rotation.z);
        }
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    for (const auto& placement : data.cylinderColliders) {
        out << "collider_cylinder "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.radius) << ' '
            << formatFloat(placement.halfHeight);
        if (hasNonZeroVec3(placement.rotation)) {
            out << " rotation "
                << formatFloat(placement.rotation.x) << ' '
                << formatFloat(placement.rotation.y) << ' '
                << formatFloat(placement.rotation.z);
        }
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    if (data.hasPlayerSpawn) {
        out << "player_spawn "
            << formatFloat(data.playerSpawn.position.x) << ' '
            << formatFloat(data.playerSpawn.position.y) << ' '
            << formatFloat(data.playerSpawn.position.z) << ' '
            << formatFloat(data.playerSpawn.fallRespawnY);
        appendNodeMetadata(out, data.playerSpawn.nodeId, data.playerSpawn.parentNodeId);
        out << '\n';
    }

    for (const auto& placement : data.archetypes) {
        out << "archetype_instance " << placement.archetypeId << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.yawDegrees);
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
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
