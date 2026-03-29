#pragma once

#include "engine/rendering/lighting/RenderLight.h"

#include <glm/glm.hpp>

#include <vector>

struct PlayerTorchComponent {
    // Visual parameters (values from RuntimeSceneRenderer k* constants)
    glm::vec3 torchColor{1.00f, 0.89f, 0.76f};
    glm::vec3 spillColor{1.00f, 0.78f, 0.46f};
    glm::vec3 haloColor{1.00f, 0.84f, 0.58f};
    glm::vec3 handGlowColor{0.98f, 0.91f, 0.82f};

    float torchRadius = 3.1f;
    float torchIntensity = 0.07f;
    float torchForwardOffset = 0.230f;
    float torchRightOffset = -0.155f;
    float torchDownOffset = 0.040f;

    glm::vec3 spillOffset{-0.030f, -0.180f, -0.060f};
    float spillRadius = 7.2f;
    float spillIntensity = 1.12f;

    float haloRadius = 5.4f;
    float haloIntensity = 0.54f;

    float handGlowRadius = 1.10f;
    float handGlowIntensity = 0.04f;
    float handGlowForwardOffset = 0.08f;
    float handGlowRightOffset = -0.14f;
    float handGlowDownOffset = 0.06f;

    float innerConeDegrees = 58.0f;
    float outerConeDegrees = 82.0f;

    // Runtime computed lights (written by updatePlayerTorch, read by RuntimeSceneRenderer)
    std::vector<RenderLight> computedLights;
};
