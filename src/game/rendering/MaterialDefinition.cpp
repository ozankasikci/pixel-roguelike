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
    if (definition.emissiveStrength.has_value()) {
        resolved.emissiveStrength = *definition.emissiveStrength;
    }
    if (definition.proceduralSource.has_value()) {
        resolved.proceduralSource = *definition.proceduralSource;
    }
    if (definition.specularLevel.has_value()) {
        resolved.specularLevel = *definition.specularLevel;
    }
    if (definition.animated.has_value()) {
        resolved.animated = *definition.animated;
    }
    if (definition.subsurface.has_value()) {
        resolved.subsurface = *definition.subsurface;
    }
    if (definition.detailBrick.has_value()) {
        resolved.detailBrick = *definition.detailBrick;
    }
    if (definition.detailWood.has_value()) {
        resolved.detailWood = *definition.detailWood;
    }
    if (definition.detailStone.has_value()) {
        resolved.detailStone = *definition.detailStone;
    }
    if (definition.detailFloor.has_value()) {
        resolved.detailFloor = *definition.detailFloor;
    }

    visiting.erase(id);
    cache.emplace(id, resolved);
    return resolved;
}

} // namespace

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
    if (token == "generated_smooth") {
        source = MaterialProceduralSource::GeneratedSmooth;
        return true;
    }
    if (token == "generated_floor") {
        source = MaterialProceduralSource::GeneratedFloor;
        return true;
    }
    if (token == "generated_ceiling") {
        source = MaterialProceduralSource::GeneratedCeiling;
        return true;
    }
    return false;
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
            // Legacy field — silently ignored; shading model is now derived from feature flags
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
        if (key == "emissive_strength") {
            definition.emissiveStrength = parseFloatRecord(tokens, path, lineNumber, key);
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
        if (key == "specular_level") {
            definition.specularLevel = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "animated" && tokens.size() == 2) {
            definition.animated = (tokens[1] == "true");
            continue;
        }
        if (key == "subsurface" && tokens.size() == 2) {
            definition.subsurface = (tokens[1] == "true");
            continue;
        }
        if (key == "detail_brick" && tokens.size() == 2) {
            definition.detailBrick = (tokens[1] == "true");
            continue;
        }
        if (key == "detail_wood" && tokens.size() == 2) {
            definition.detailWood = (tokens[1] == "true");
            continue;
        }
        if (key == "detail_stone" && tokens.size() == 2) {
            definition.detailStone = (tokens[1] == "true");
            continue;
        }
        if (key == "detail_floor" && tokens.size() == 2) {
            definition.detailFloor = (tokens[1] == "true");
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
    case MaterialProceduralSource::GeneratedSmooth:
        return "generated_smooth";
    case MaterialProceduralSource::GeneratedFloor:
        return "generated_floor";
    case MaterialProceduralSource::GeneratedCeiling:
        return "generated_ceiling";
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
    writeOptionalFloat(out, "emissive_strength", definition.emissiveStrength);
    if (definition.proceduralSource.has_value()) {
        out << "procedural_source " << materialProceduralSourceToken(*definition.proceduralSource) << '\n';
    }
    writeOptionalFloat(out, "specular_level", definition.specularLevel);
    if (definition.animated.has_value()) {
        out << "animated " << (*definition.animated ? "true" : "false") << '\n';
    }
    if (definition.subsurface.has_value()) {
        out << "subsurface " << (*definition.subsurface ? "true" : "false") << '\n';
    }
    if (definition.detailBrick.has_value()) {
        out << "detail_brick " << (*definition.detailBrick ? "true" : "false") << '\n';
    }
    if (definition.detailWood.has_value()) {
        out << "detail_wood " << (*definition.detailWood ? "true" : "false") << '\n';
    }
    if (definition.detailStone.has_value()) {
        out << "detail_stone " << (*definition.detailStone ? "true" : "false") << '\n';
    }
    if (definition.detailFloor.has_value()) {
        out << "detail_floor " << (*definition.detailFloor ? "true" : "false") << '\n';
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
