#pragma once

#include "engine/ecs/GameRegistry.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <string>

struct RuntimeEnvironmentOverride {
    EnvironmentDefinition definition;
};

struct ActiveEnvironmentState {
    std::string levelId;
    std::string environmentId;
};

struct RuntimeEnvironmentSyncState {
    bool hasApplied = false;
    bool usedOverride = false;
    std::string levelId;
    std::string environmentId;
    EnvironmentProfile profile = EnvironmentProfile::Default;
};

void syncSkySunFromDirectional(DebugParams& params);
void applyEnvironmentSettings(DebugParams& params,
                              const EnvironmentRenderSettings& settings,
                              bool preserveDebugOverrides);
void applyEnvironmentProfile(DebugParams& params,
                             EnvironmentProfile profile,
                             bool preserveDebugOverrides);
void applyEnvironmentDefinition(DebugParams& params,
                                const EnvironmentDefinition& definition,
                                bool preserveDebugOverrides);
void syncEnvironmentFromRegistry(GameRegistry& registry,
                                 DebugParams& params,
                                 RuntimeEnvironmentSyncState* syncState = nullptr,
                                 bool preserveDebugOverrides = false);
