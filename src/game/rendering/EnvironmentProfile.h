#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "engine/rendering/post/PostProcessParams.h"

#include <string>

enum class EnvironmentProfile {
    Neutral = 0,
    DungeonTorch = 1,
    SunlitMeadow = 2,
    MountainDusk = 3,
    ArcaneField = 4,
    CathedralArcade = 5,
    CloisterDaylight = 6,
};

struct EnvironmentRenderSettings {
    EnvironmentProfile profile = EnvironmentProfile::Neutral;
    PostProcessParams post;
    SkySettings sky;
    LightingEnvironment lighting;
};

struct ActiveEnvironmentProfile {
    std::string levelId;
    std::string environmentId;
    EnvironmentProfile profile = EnvironmentProfile::Neutral;
};

bool tryParseEnvironmentProfileToken(const std::string& token, EnvironmentProfile& profile);
const char* environmentProfileName(EnvironmentProfile profile);
EnvironmentRenderSettings makeEnvironmentRenderSettings(EnvironmentProfile profile);
