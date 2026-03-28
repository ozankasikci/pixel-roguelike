#include "game/rendering/MaterialDefinition.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
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

glm::vec3 parseVec3Record(const std::vector<std::string>& tokens,
                          const std::string& path,
                          int lineNumber,
                          const std::string& label) {
    if (tokens.size() != 4) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
}

glm::vec2 parseVec2Record(const std::vector<std::string>& tokens,
                          const std::string& path,
                          int lineNumber,
                          const std::string& label) {
    if (tokens.size() != 3) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return glm::vec2(std::stof(tokens[1]), std::stof(tokens[2]));
}

float parseFloatRecord(const std::vector<std::string>& tokens,
                       const std::string& path,
                       int lineNumber,
                       const std::string& label) {
    if (tokens.size() != 2) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return std::stof(tokens[1]);
}

std::vector<std::string> tokenizeRecord(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

ResolvedMaterialDefinition resolveMaterialDefinitionRecursive(
    const std::string& id,
    const std::unordered_map<std::string, MaterialDefinition>& definitions,
    std::unordered_map<std::string, ResolvedMaterialDefinition>& cache,
    std::unordered_set<std::string>& visiting) {
    auto cached = cache.find(id);
    if (cached != cache.end()) {
        return cached->second;
    }

    auto defIt = definitions.find(id);
    if (defIt == definitions.end()) {
        throw std::runtime_error("Unknown material id: " + id);
    }

    if (!visiting.insert(id).second) {
        throw std::runtime_error("Material inheritance cycle detected at: " + id);
    }

    const MaterialDefinition& definition = defIt->second;
    ResolvedMaterialDefinition resolved;
    resolved.id = definition.id;

    if (definition.parent.has_value()) {
        auto parent = resolveMaterialDefinitionRecursive(*definition.parent, definitions, cache, visiting);
        resolved = parent;
        resolved.id = definition.id;
        resolved.parent = *definition.parent;
    } else if (!definition.shadingModel.has_value()) {
        throw std::runtime_error("Root material missing shading_model: " + definition.id);
    }

    if (definition.shadingModel.has_value()) {
        resolved.shadingModel = *definition.shadingModel;
    }
    if (definition.albedoMapPath.has_value()) {
        resolved.albedoMapPath = *definition.albedoMapPath;
    }
    if (definition.normalMapPath.has_value()) {
        resolved.normalMapPath = *definition.normalMapPath;
    }
    if (definition.roughnessMapPath.has_value()) {
        resolved.roughnessMapPath = *definition.roughnessMapPath;
    }
    if (definition.aoMapPath.has_value()) {
        resolved.aoMapPath = *definition.aoMapPath;
    }
    if (definition.baseColor.has_value()) {
        resolved.baseColor = *definition.baseColor;
    }
    if (definition.uvMode.has_value()) {
        resolved.uvMode = *definition.uvMode;
    }
    if (definition.uvScale.has_value()) {
        resolved.uvScale = *definition.uvScale;
    }
    if (definition.normalStrength.has_value()) {
        resolved.normalStrength = *definition.normalStrength;
    }
    if (definition.roughnessScale.has_value()) {
        resolved.roughnessScale = *definition.roughnessScale;
    }
    if (definition.roughnessBias.has_value()) {
        resolved.roughnessBias = *definition.roughnessBias;
    }
    if (definition.metalness.has_value()) {
        resolved.metalness = *definition.metalness;
    }
    if (definition.aoStrength.has_value()) {
        resolved.aoStrength = *definition.aoStrength;
    }
    if (definition.lightTintResponse.has_value()) {
        resolved.lightTintResponse = *definition.lightTintResponse;
    }
    if (definition.proceduralSource.has_value()) {
        resolved.proceduralSource = *definition.proceduralSource;
    }

    visiting.erase(id);
    cache.emplace(id, resolved);
    return resolved;
}

} // namespace

bool tryParseMaterialKindToken(const std::string& token, MaterialKind& materialKind) {
    if (token == "stone") {
        materialKind = MaterialKind::Stone;
        return true;
    }
    if (token == "wood") {
        materialKind = MaterialKind::Wood;
        return true;
    }
    if (token == "metal") {
        materialKind = MaterialKind::Metal;
        return true;
    }
    if (token == "wax") {
        materialKind = MaterialKind::Wax;
        return true;
    }
    if (token == "moss") {
        materialKind = MaterialKind::Moss;
        return true;
    }
    if (token == "viewmodel") {
        materialKind = MaterialKind::Viewmodel;
        return true;
    }
    if (token == "floor") {
        materialKind = MaterialKind::Floor;
        return true;
    }
    if (token == "brick") {
        materialKind = MaterialKind::Brick;
        return true;
    }
    return false;
}

bool tryParseMaterialUvModeToken(const std::string& token, MaterialUvMode& uvMode) {
    if (token == "mesh") {
        uvMode = MaterialUvMode::Mesh;
        return true;
    }
    if (token == "world_projected") {
        uvMode = MaterialUvMode::WorldProjected;
        return true;
    }
    return false;
}

bool tryParseMaterialProceduralSourceToken(const std::string& token, MaterialProceduralSource& source) {
    if (token == "none") {
        source = MaterialProceduralSource::None;
        return true;
    }
    if (token == "generated_brick") {
        source = MaterialProceduralSource::GeneratedBrick;
        return true;
    }
    if (token == "generated_stone") {
        source = MaterialProceduralSource::GeneratedStone;
        return true;
    }
    return false;
}

std::string_view defaultMaterialIdForKind(MaterialKind kind) {
    switch (kind) {
    case MaterialKind::Stone:
        return "stone_default";
    case MaterialKind::Wood:
        return "wood_default";
    case MaterialKind::Metal:
        return "metal_default";
    case MaterialKind::Wax:
        return "wax_default";
    case MaterialKind::Moss:
        return "moss_default";
    case MaterialKind::Viewmodel:
        return "viewmodel_default";
    case MaterialKind::Floor:
        return "floor_default";
    case MaterialKind::Brick:
        return "brick_default";
    }
    return "stone_default";
}

MaterialDefinition loadMaterialDefinitionAsset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open material definition: " + path);
    }

    MaterialDefinition definition;
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
        if (key == "parent" && tokens.size() == 2) {
            definition.parent = tokens[1];
            continue;
        }
        if (key == "shading_model" && tokens.size() == 2) {
            MaterialKind kind;
            if (!tryParseMaterialKindToken(tokens[1], kind)) {
                throwParseError(path, lineNumber, "unknown shading_model");
            }
            definition.shadingModel = kind;
            continue;
        }
        if (key == "albedo_map" && tokens.size() == 2) {
            definition.albedoMapPath = tokens[1];
            continue;
        }
        if (key == "normal_map" && tokens.size() == 2) {
            definition.normalMapPath = tokens[1];
            continue;
        }
        if (key == "roughness_map" && tokens.size() == 2) {
            definition.roughnessMapPath = tokens[1];
            continue;
        }
        if (key == "ao_map" && tokens.size() == 2) {
            definition.aoMapPath = tokens[1];
            continue;
        }
        if (key == "base_color") {
            definition.baseColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "uv_mode" && tokens.size() == 2) {
            MaterialUvMode uvMode;
            if (!tryParseMaterialUvModeToken(tokens[1], uvMode)) {
                throwParseError(path, lineNumber, "unknown uv_mode");
            }
            definition.uvMode = uvMode;
            continue;
        }
        if (key == "uv_scale") {
            definition.uvScale = parseVec2Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "normal_strength") {
            definition.normalStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "roughness_scale") {
            definition.roughnessScale = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "roughness_bias") {
            definition.roughnessBias = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "metalness") {
            definition.metalness = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "ao_strength") {
            definition.aoStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "light_tint_response") {
            definition.lightTintResponse = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "procedural_source" && tokens.size() == 2) {
            MaterialProceduralSource source;
            if (!tryParseMaterialProceduralSourceToken(tokens[1], source)) {
                throwParseError(path, lineNumber, "unknown procedural_source");
            }
            definition.proceduralSource = source;
            continue;
        }

        throwParseError(path, lineNumber, "invalid material definition record");
    }

    if (definition.id.empty()) {
        throw std::runtime_error("Material definition missing id: " + path);
    }

    return definition;
}

ResolvedMaterialDefinition resolveMaterialDefinition(
    const std::string& id,
    const std::unordered_map<std::string, MaterialDefinition>& definitions) {
    std::unordered_map<std::string, ResolvedMaterialDefinition> cache;
    std::unordered_set<std::string> visiting;
    return resolveMaterialDefinitionRecursive(id, definitions, cache, visiting);
}

namespace {

std::string materialKindToken(MaterialKind kind) {
    switch (kind) {
    case MaterialKind::Stone:
        return "stone";
    case MaterialKind::Wood:
        return "wood";
    case MaterialKind::Metal:
        return "metal";
    case MaterialKind::Wax:
        return "wax";
    case MaterialKind::Moss:
        return "moss";
    case MaterialKind::Viewmodel:
        return "viewmodel";
    case MaterialKind::Floor:
        return "floor";
    case MaterialKind::Brick:
        return "brick";
    }
    return "stone";
}

std::string materialUvModeToken(MaterialUvMode uvMode) {
    switch (uvMode) {
    case MaterialUvMode::Mesh:
        return "mesh";
    case MaterialUvMode::WorldProjected:
        return "world_projected";
    }
    return "mesh";
}

std::string materialProceduralSourceToken(MaterialProceduralSource source) {
    switch (source) {
    case MaterialProceduralSource::None:
        return "none";
    case MaterialProceduralSource::GeneratedBrick:
        return "generated_brick";
    case MaterialProceduralSource::GeneratedStone:
        return "generated_stone";
    }
    return "none";
}

void writeOptionalPath(std::ostringstream& out,
                       std::string_view key,
                       const std::optional<std::string>& value) {
    if (value.has_value() && !value->empty()) {
        out << key << ' ' << *value << '\n';
    }
}

void writeOptionalFloat(std::ostringstream& out,
                        std::string_view key,
                        const std::optional<float>& value) {
    if (value.has_value()) {
        out << key << ' ' << *value << '\n';
    }
}

} // namespace

std::string serializeMaterialDefinitionAsset(const MaterialDefinition& definition) {
    if (definition.id.empty()) {
        throw std::runtime_error("Cannot serialize material definition without id");
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "id " << definition.id << '\n';
    if (definition.parent.has_value() && !definition.parent->empty()) {
        out << "parent " << *definition.parent << '\n';
    }
    if (definition.shadingModel.has_value()) {
        out << "shading_model " << materialKindToken(*definition.shadingModel) << '\n';
    }
    writeOptionalPath(out, "albedo_map", definition.albedoMapPath);
    writeOptionalPath(out, "normal_map", definition.normalMapPath);
    writeOptionalPath(out, "roughness_map", definition.roughnessMapPath);
    writeOptionalPath(out, "ao_map", definition.aoMapPath);
    if (definition.baseColor.has_value()) {
        out << "base_color "
            << definition.baseColor->x << ' '
            << definition.baseColor->y << ' '
            << definition.baseColor->z << '\n';
    }
    if (definition.uvMode.has_value()) {
        out << "uv_mode " << materialUvModeToken(*definition.uvMode) << '\n';
    }
    if (definition.uvScale.has_value()) {
        out << "uv_scale "
            << definition.uvScale->x << ' '
            << definition.uvScale->y << '\n';
    }
    writeOptionalFloat(out, "normal_strength", definition.normalStrength);
    writeOptionalFloat(out, "roughness_scale", definition.roughnessScale);
    writeOptionalFloat(out, "roughness_bias", definition.roughnessBias);
    writeOptionalFloat(out, "metalness", definition.metalness);
    writeOptionalFloat(out, "ao_strength", definition.aoStrength);
    writeOptionalFloat(out, "light_tint_response", definition.lightTintResponse);
    if (definition.proceduralSource.has_value()) {
        out << "procedural_source " << materialProceduralSourceToken(*definition.proceduralSource) << '\n';
    }
    return out.str();
}

void saveMaterialDefinitionAsset(const std::string& path, const MaterialDefinition& definition) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to save material definition: " + path);
    }
    file << serializeMaterialDefinitionAsset(definition);
}
