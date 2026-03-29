#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "engine/rendering/post/PostProcessParams.h"

#include <string>

enum class EnvironmentProfile {
    Default = 0,
};

struct EnvironmentRenderSettings {
    PostProcessParams post;
    SkySettings sky;
    LightingEnvironment lighting;
};

struct ActiveEnvironmentProfile {
    std::string levelId;
    std::string environmentId;
    EnvironmentProfile profile = EnvironmentProfile::Default;
};

bool tryParseEnvironmentProfileToken(const std::string& token, EnvironmentProfile& profile);
const char* environmentProfileName(EnvironmentProfile profile);
EnvironmentRenderSettings makeEnvironmentRenderSettings(EnvironmentProfile profile);
EnvironmentRenderSettings makeDefaultEnvironmentRenderSettings();
