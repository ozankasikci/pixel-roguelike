#include "game/rendering/EnvironmentProfile.h"

namespace {

constexpr glm::vec3 kDefaultSunDirection = glm::vec3(0.28f, 0.91f, 0.18f);
constexpr glm::vec3 kDefaultSunColor = glm::vec3(1.00f, 0.95f, 0.88f);

SkySettings makeDefaultSky() {
    SkySettings sky;
    sky.zenithColor = glm::vec3(0.20f, 0.32f, 0.54f);
    sky.horizonColor = glm::vec3(0.78f, 0.82f, 0.82f);
    sky.groundHazeColor = glm::vec3(0.68f, 0.66f, 0.60f);
    sky.sunSize = 0.014f;
    sky.sunGlow = 0.10f;
    sky.cloudLayerAPath = "assets/skies/clouds_soft.tga";
    sky.cloudTint = glm::vec3(0.98f, 0.99f, 1.00f);
    sky.cloudScale = 0.95f;
    sky.cloudSpeed = 0.0018f;
    sky.cloudCoverage = 0.22f;
    sky.cloudParallax = 0.016f;
    return sky;
}

DirectionalLightSlot makeSunSlot(const glm::vec3& direction,
                                 const glm::vec3& color,
                                 float intensity,
                                 bool enabled = true) {
    DirectionalLightSlot slot;
    slot.enabled = enabled && intensity > 0.001f;
    if (glm::dot(direction, direction) > 0.0001f) {
        slot.direction = glm::normalize(direction);
    }
    slot.color = color;
    slot.intensity = intensity;
    return slot;
}

DirectionalLightSlot makeFillSlot(const glm::vec3& direction,
                                  const glm::vec3& color,
                                  float intensity,
                                  bool enabled = true) {
    DirectionalLightSlot slot;
    slot.enabled = enabled && intensity > 0.001f;
    if (glm::dot(direction, direction) > 0.0001f) {
        slot.direction = glm::normalize(direction);
    }
    slot.color = color;
    slot.intensity = intensity;
    return slot;
}

} // namespace

bool tryParseEnvironmentProfileToken(const std::string& token, EnvironmentProfile& profile) {
    if (token == "default"
        || token == "game_ready_neutral"
        || token == "neutral"
        || token == "dungeon_torch"
        || token == "sunlit_meadow"
        || token == "mountain_dusk"
        || token == "arcane_field"
        || token == "cathedral_arcade"
        || token == "cloister_daylight") {
        profile = EnvironmentProfile::Default;
        return true;
    }
    return false;
}

const char* environmentProfileName(EnvironmentProfile profile) {
    switch (profile) {
    case EnvironmentProfile::Default:
        return "default";
    }
    return "default";
}

EnvironmentRenderSettings makeEnvironmentRenderSettings(EnvironmentProfile /*profile*/) {
    return makeDefaultEnvironmentRenderSettings();
}

EnvironmentRenderSettings makeDefaultEnvironmentRenderSettings() {
    EnvironmentRenderSettings settings;
    settings.sky = makeDefaultSky();
    settings.post.enableEdges = false;
    settings.post.enableToneMap = true;
    settings.post.enableBloom = true;
    settings.post.enableVignette = true;
    settings.post.enableGrain = false;
    settings.post.enableScanlines = false;
    settings.post.enableSharpen = false;
    settings.post.enableFxaa = true;
    settings.post.toneMapMode = 1;
    settings.post.fogDensity = 0.018f;
    settings.post.fogStart = 26.0f;
    settings.post.fogNearColor = glm::vec3(0.32f, 0.32f, 0.30f);
    settings.post.fogFarColor = glm::vec3(0.52f, 0.52f, 0.50f);
    settings.post.exposure = 1.0f;
    settings.post.gamma = 1.0f;
    settings.post.contrast = 1.05f;
    settings.post.saturation = 0.95f;
    settings.post.bloomThreshold = 0.80f;
    settings.post.bloomIntensity = 0.10f;
    settings.post.bloomRadius = 1.20f;
    settings.post.ssaoHalfResolution = false;
    settings.post.vignetteStrength = 0.08f;
    settings.post.vignetteSoftness = 0.85f;
    settings.post.splitToneStrength = 0.05f;
    settings.post.splitToneBalance = 0.55f;
    settings.post.shadowTint = glm::vec3(0.92f, 0.92f, 0.94f);
    settings.post.highlightTint = glm::vec3(0.99f, 0.97f, 0.92f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.40f, 0.40f, 0.40f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.12f, 0.11f, 0.10f);
    settings.lighting.hemisphereStrength = 0.38f;
    settings.lighting.enableDirectionalLights = true;
    settings.lighting.enableShadows = true;
    settings.lighting.sun = makeSunSlot(kDefaultSunDirection, kDefaultSunColor, 1.00f);
    settings.lighting.fill = makeFillSlot(glm::vec3(-0.18f, 0.78f, -0.42f),
                                          glm::vec3(0.74f, 0.80f, 0.90f),
                                          0.10f);
    return settings;
}
