#pragma once

#include <glm/glm.hpp>

enum class LightType {
    Point = 0,
    Spot = 1,
    Directional = 2,
};

constexpr int kMaxRenderLights = 32;
constexpr int kMaxShadowedSpotLights = 2;

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
    bool castsShadows = false;
    int shadowIndex = -1;
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
};
