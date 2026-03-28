#pragma once

#include "engine/ui/ImGuiLayer.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <entt/entt.hpp>

#include <string>

struct RuntimeEnvironmentOverride {
    EnvironmentDefinition definition;
};

struct RuntimeEnvironmentSyncState {
    bool hasApplied = false;
    bool usedOverride = false;
    std::string levelId;
    std::string environmentId;
    EnvironmentProfile profile = EnvironmentProfile::Neutral;
};

void syncSkySunFromDirectional(DebugParams& params);
void applyEnvironmentSettings(DebugParams& params,
                              const EnvironmentRenderSettings& settings,
                              bool preserveDebugOverrides);
void applyEnvironmentDefinition(DebugParams& params,
                                const EnvironmentDefinition& definition,
                                bool preserveDebugOverrides);
void applyEnvironmentProfile(DebugParams& params,
                             EnvironmentProfile profile,
                             bool preserveDebugOverrides);
void syncEnvironmentFromRegistry(entt::registry& registry,
                                 DebugParams& params,
                                 RuntimeEnvironmentSyncState* syncState = nullptr,
                                 bool preserveDebugOverrides = false);
