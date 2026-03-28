#pragma once

#include "game/rendering/MaterialKind.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

enum class MaterialUvMode {
    Mesh = 0,
    WorldProjected = 2,
};

enum class MaterialProceduralSource {
    None = 0,
    GeneratedBrick = 1,
    GeneratedStone = 2,
};

struct MaterialDefinition {
    std::string id;
    std::optional<std::string> parent;
    std::optional<MaterialKind> shadingModel;
    std::optional<std::string> albedoMapPath;
    std::optional<std::string> normalMapPath;
    std::optional<std::string> roughnessMapPath;
    std::optional<std::string> aoMapPath;
    std::optional<glm::vec3> baseColor;
    std::optional<MaterialUvMode> uvMode;
    std::optional<glm::vec2> uvScale;
    std::optional<float> normalStrength;
    std::optional<float> roughnessScale;
    std::optional<float> roughnessBias;
    std::optional<float> metalness;
    std::optional<float> aoStrength;
    std::optional<float> lightTintResponse;
    std::optional<MaterialProceduralSource> proceduralSource;
};

struct ResolvedMaterialDefinition {
    std::string id;
    std::string parent;
    MaterialKind shadingModel = MaterialKind::Stone;
    std::string albedoMapPath;
    std::string normalMapPath;
    std::string roughnessMapPath;
    std::string aoMapPath;
    glm::vec3 baseColor{1.0f};
    MaterialUvMode uvMode = MaterialUvMode::Mesh;
    glm::vec2 uvScale{1.0f, 1.0f};
    float normalStrength = 1.0f;
    float roughnessScale = 1.0f;
    float roughnessBias = 0.0f;
    float metalness = 0.0f;
    float aoStrength = 1.0f;
    float lightTintResponse = 0.18f;
    MaterialProceduralSource proceduralSource = MaterialProceduralSource::None;
};

MaterialDefinition loadMaterialDefinitionAsset(const std::string& path);
ResolvedMaterialDefinition resolveMaterialDefinition(
    const std::string& id,
    const std::unordered_map<std::string, MaterialDefinition>& definitions);
std::string serializeMaterialDefinitionAsset(const MaterialDefinition& definition);
void saveMaterialDefinitionAsset(const std::string& path, const MaterialDefinition& definition);

bool tryParseMaterialKindToken(const std::string& token, MaterialKind& materialKind);
bool tryParseMaterialUvModeToken(const std::string& token, MaterialUvMode& uvMode);
bool tryParseMaterialProceduralSourceToken(const std::string& token, MaterialProceduralSource& source);

std::string_view defaultMaterialIdForKind(MaterialKind kind);
