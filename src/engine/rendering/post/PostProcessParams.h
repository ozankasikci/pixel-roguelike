#pragma once

#include "engine/rendering/post/SkySettings.h"

#include <glm/glm.hpp>

struct PostProcessParams {
    bool enableSky       = true;
    bool enableEdges     = false;
    bool enableFog       = true;
    bool enableToneMap   = true;
    bool enableBloom     = true;
    bool enableVignette  = true;
    bool enableGrain     = true;
    bool enableScanlines = false;
    bool enableSharpen   = false;
    int  debugViewMode   = 0;      // 0=final, 1=scene color, 2=normals, 3=depth, 4=sky, 5=sun direct, 6=sun shadow, 7=csm uv, 8=cascade index
    int  toneMapMode     = 1;      // 0=linear, 1=aces fitted
    float edgeThreshold   = 0.17f;
    float fogDensity      = 0.040f;
    float fogStart        = 14.0f;
    float depthViewScale  = 0.080f;
    float exposure        = 1.4f;
    float gamma           = 1.00f;
    float contrast        = 1.06f;
    float saturation      = 0.96f;
    float bloomThreshold  = 0.58f;
    float bloomIntensity  = 0.14f;
    float bloomRadius     = 1.80f;
    bool enableSsao     = true;
    bool ssaoHalfResolution = true;
    float ssaoRadius    = 0.5f;
    float ssaoBias      = 0.025f;
    float ssaoStrength  = 0.5f;
    float ssaoFadeStart = 18.0f;
    float ssaoFadeEnd   = 48.0f;
    float csmLambda     = 0.5f;   // PSSM cascade split blend: 0=uniform linear, 1=fully logarithmic
    float vignetteStrength = 0.24f;
    float vignetteSoftness = 0.68f;
    float grainAmount      = 0.035f;
    float scanlineAmount   = 0.08f;
    float scanlineDensity  = 1.0f;
    float sharpenAmount    = 0.18f;
    float splitToneStrength = 0.12f;
    float splitToneBalance  = 0.50f;
    float timeSeconds       = 0.0f;
    glm::vec3 sunDirection  = glm::vec3(0.25f, 0.92f, 0.28f); // runtime-only, synced from lighting
    glm::vec3 sunColor      = glm::vec3(1.0f, 0.94f, 0.82f);  // runtime-only, synced from lighting
    glm::vec3 fogNearColor  = glm::vec3(0.06f, 0.055f, 0.05f);
    glm::vec3 fogFarColor   = glm::vec3(0.13f, 0.12f, 0.11f);
    glm::vec3 shadowTint    = glm::vec3(0.86f, 0.88f, 0.92f);
    glm::vec3 highlightTint = glm::vec3(0.98f, 0.92f, 0.82f);
    float nearPlane         = 0.1f;
    float farPlane          = 100.0f;
    glm::mat4 inverseViewProjection = glm::mat4(1.0f);
    SkySettings sky{};
};
