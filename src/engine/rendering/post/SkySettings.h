#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>

struct SkySettings {
    bool enabled = false;

    glm::vec3 zenithColor{0.10f, 0.16f, 0.24f};
    glm::vec3 horizonColor{0.52f, 0.60f, 0.70f};
    glm::vec3 groundHazeColor{0.70f, 0.68f, 0.62f};

    glm::vec3 sunDirection{0.25f, 0.92f, 0.28f};
    glm::vec3 sunColor{1.0f, 0.94f, 0.82f};
    float sunSize = 0.020f;
    float sunGlow = 0.24f;

    std::string panoramaPath;
    glm::vec3 panoramaTint{1.0f, 1.0f, 1.0f};
    float panoramaStrength = 0.0f;
    float panoramaYawOffset = 0.0f;

    std::array<std::string, 6> cubemapFacePaths{};
    glm::vec3 cubemapTint{1.0f, 1.0f, 1.0f};
    float cubemapStrength = 0.0f;

    bool moonEnabled = false;
    glm::vec3 moonDirection{-0.30f, 0.85f, -0.22f};
    glm::vec3 moonColor{0.72f, 0.82f, 1.0f};
    float moonSize = 0.018f;
    float moonGlow = 0.12f;

    std::string cloudLayerAPath;
    std::string cloudLayerBPath;
    std::string horizonLayerPath;
    glm::vec3 cloudTint{1.0f, 1.0f, 1.0f};
    float cloudScale = 1.0f;
    float cloudSpeed = 0.004f;
    float cloudCoverage = 0.52f;
    float cloudParallax = 0.018f;

    glm::vec3 horizonTint{1.0f, 1.0f, 1.0f};
    float horizonHeight = 0.52f;
    float horizonFade = 0.22f;
};
