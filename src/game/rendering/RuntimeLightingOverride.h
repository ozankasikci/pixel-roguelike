#pragma once
#include "engine/rendering/lighting/RenderLight.h"
#include <glm/glm.hpp>

// Mutable torch parameters exposed through the debug overlay.
// Default values match the constexpr constants in RuntimeSceneRenderer.cpp exactly.
struct PlayerTorchOverride {
    bool enabled = true;
    float masterIntensity = 1.0f;  // Scales all 4 torch lights together

    // Main spotlight
    glm::vec3 torchColor{1.00f, 0.89f, 0.76f};
    float torchIntensity = 0.15f;
    float torchRadius = 3.1f;
    float torchInnerConeDegrees = 58.0f;
    float torchOuterConeDegrees = 82.0f;

    // Spill point light
    glm::vec3 spillColor{1.00f, 0.78f, 0.46f};
    float spillIntensity = 2.5f;
    float spillRadius = 7.2f;

    // Halo point light
    glm::vec3 haloColor{1.00f, 0.84f, 0.58f};
    float haloIntensity = 1.2f;
    float haloRadius = 5.4f;

    // Hand glow point light
    glm::vec3 handGlowColor{0.98f, 0.91f, 0.82f};
    float handGlowIntensity = 0.08f;
    float handGlowRadius = 1.10f;
};

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
    PlayerTorchOverride torch;
};
