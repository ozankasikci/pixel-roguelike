#pragma once

#include <glm/glm.hpp>

enum class LightType {
    Point = 0,
    Spot = 1,
    Directional = 2,
    AreaRect = 3,
    Tube = 4,
};

constexpr int kMaxRenderLights = 32;
constexpr int kMaxShadowedSpotLights = 8;

struct DirectionalLightSlot {
    bool enabled = false;
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct RenderLight {
    LightType type = LightType::Point;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float radius = 10.0f;
    float intensity = 1.0f;
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 30.0f;
    // Area light fields (AreaRect and Tube):
    glm::vec3 right{1.0f, 0.0f, 0.0f};   // light's local X axis
    glm::vec3 up{0.0f, 1.0f, 0.0f};      // light's local Y axis
    float width = 1.0f;                    // half-width for AreaRect, half-length for Tube
    float height = 0.5f;                   // half-height for AreaRect, tube radius for Tube
    bool doubleSided = false;              // AreaRect: emit from both sides
    bool castsShadows = false;
    int shadowIndex = -1;
};

// Mutable torch parameters — source of truth for player torch lighting.
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

struct LightingEnvironment {
    glm::vec3 hemisphereSkyColor{0.17f, 0.18f, 0.20f};
    glm::vec3 hemisphereGroundColor{0.06f, 0.05f, 0.045f};
    float hemisphereStrength = 0.32f;
    bool enableDirectionalLights = true;
    DirectionalLightSlot sun{
        true,
        glm::vec3(0.24f, 0.88f, 0.26f),
        glm::vec3(0.98f, 0.96f, 0.94f),
        1.0f
    };
    DirectionalLightSlot fill{
        false,
        glm::vec3(-0.22f, 0.74f, -0.36f),
        glm::vec3(0.72f, 0.80f, 0.92f),
        0.18f
    };
    bool enableShadows = true;
    float shadowBias = 0.0018f;
    float shadowNormalBias = 0.03f;
    PlayerTorchOverride torch;
};
