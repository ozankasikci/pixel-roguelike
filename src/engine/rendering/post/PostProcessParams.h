#pragma once

#include <glm/glm.hpp>

struct PostProcessParams {
    bool enableDither    = true;
    bool enableEdges     = true;
    bool enableFog       = true;
    bool enableToneMap   = true;
    bool enableBloom     = true;
    bool enableVignette  = true;
    bool enableGrain     = true;
    bool enableScanlines = false;
    bool enableSharpen   = false;
    int  debugViewMode   = 0;      // 0=final, 1=scene color, 2=normals, 3=depth
    int  toneMapMode     = 1;      // 0=linear, 1=aces fitted
    float thresholdBias   = 0.028f;
    float patternScale    = 0.0f;  // 0 = match internal resolution grid
    float edgeThreshold   = 0.17f;
    float fogDensity      = 0.090f;
    float fogStart        = 10.5f;
    float depthViewScale  = 0.080f;
    float exposure        = 1.08f;
    float gamma           = 1.0f;
    float contrast        = 1.08f;
    float saturation      = 0.92f;
    float bloomThreshold  = 0.62f;
    float bloomIntensity  = 0.18f;
    float bloomRadius     = 1.75f;
    float vignetteStrength = 0.28f;
    float vignetteSoftness = 0.58f;
    float grainAmount      = 0.035f;
    float scanlineAmount   = 0.08f;
    float scanlineDensity  = 1.0f;
    float sharpenAmount    = 0.18f;
    float splitToneStrength = 0.18f;
    float splitToneBalance  = 0.52f;
    float timeSeconds       = 0.0f;
    glm::vec3 shadowTint    = glm::vec3(0.88f, 0.92f, 1.00f);
    glm::vec3 highlightTint = glm::vec3(1.00f, 0.94f, 0.84f);
    float nearPlane         = 0.1f;
    float farPlane          = 100.0f;
};
