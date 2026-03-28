#pragma once

#include "game/rendering/EnvironmentProfile.h"

#include <string>
#include <unordered_map>

struct EnvironmentDefinition {
    std::string id;
    PostProcessParams post;
    SkySettings sky;
    LightingEnvironment lighting;
};

EnvironmentDefinition makeEnvironmentDefinition(EnvironmentProfile profile);
EnvironmentRenderSettings makeEnvironmentRenderSettings(const EnvironmentDefinition& definition);

EnvironmentDefinition loadEnvironmentDefinitionAsset(const std::string& path);
std::string serializeEnvironmentDefinitionAsset(const EnvironmentDefinition& definition);
void saveEnvironmentDefinitionAsset(const std::string& path, const EnvironmentDefinition& definition);
