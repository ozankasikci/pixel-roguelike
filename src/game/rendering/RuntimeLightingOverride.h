#pragma once
#include "engine/rendering/lighting/RenderLight.h"
#include <glm/glm.hpp>

// Runtime lighting overrides for debug tuning.
// Drives shadow, hemisphere, and directional light settings from the debug UI.
struct RuntimeLightingOverride {
    bool shadowsEnabled = true;
    int shadowMapResolutionIndex = 1;  // 0=512, 1=1024, 2=2048
    float shadowBias = 0.0018f;
    float shadowNormalBias = 0.03f;
    glm::vec3 hemisphereSkyColor{0.17f, 0.18f, 0.20f};
    glm::vec3 hemisphereGroundColor{0.06f, 0.05f, 0.045f};
    float hemisphereStrength = 0.32f;
    bool enableDirectionalLights = true;
    DirectionalLightSlot sunDirectional{
        true,
        glm::vec3(0.24f, 0.88f, 0.26f),
        glm::vec3(0.98f, 0.96f, 0.94f),
        1.0f
    };
    DirectionalLightSlot fillDirectional{
        false,
        glm::vec3(-0.22f, 0.74f, -0.36f),
        glm::vec3(0.72f, 0.80f, 0.92f),
        0.18f
    };
};
