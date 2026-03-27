#include "game/rendering/EnvironmentProfile.h"

namespace {

EnvironmentRenderSettings makeNeutralSettings() {
    EnvironmentRenderSettings settings;
    settings.profile = EnvironmentProfile::Neutral;
    settings.post.paletteVariant = 0;
    settings.post.fogDensity = 0.040f;
    settings.post.fogStart = 14.0f;
    settings.post.fogNearColor = glm::vec3(0.06f, 0.055f, 0.05f);
    settings.post.fogFarColor = glm::vec3(0.13f, 0.12f, 0.11f);
    settings.post.exposure = 1.02f;
    settings.post.gamma = 1.0f;
    settings.post.contrast = 1.06f;
    settings.post.saturation = 0.96f;
    settings.post.bloomThreshold = 0.58f;
    settings.post.bloomIntensity = 0.14f;
    settings.post.bloomRadius = 1.8f;
    settings.post.vignetteStrength = 0.24f;
    settings.post.vignetteSoftness = 0.68f;
    settings.post.splitToneStrength = 0.12f;
    settings.post.splitToneBalance = 0.50f;
    settings.post.shadowTint = glm::vec3(0.86f, 0.88f, 0.92f);
    settings.post.highlightTint = glm::vec3(0.98f, 0.92f, 0.82f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.16f, 0.17f, 0.19f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.06f, 0.055f, 0.05f);
    settings.lighting.hemisphereStrength = 0.30f;
    settings.lighting.enableDirectionalLights = true;
    settings.lighting.directionalIntensityScale = 1.0f;
    return settings;
}

EnvironmentRenderSettings makeDungeonTorchSettings() {
    EnvironmentRenderSettings settings = makeNeutralSettings();
    settings.profile = EnvironmentProfile::DungeonTorch;
    settings.post.paletteVariant = 1;
    settings.post.fogNearColor = glm::vec3(0.07f, 0.05f, 0.03f);
    settings.post.fogFarColor = glm::vec3(0.16f, 0.11f, 0.06f);
    settings.post.fogDensity = 0.060f;
    settings.post.fogStart = 12.0f;
    settings.post.exposure = 1.06f;
    settings.post.gamma = 0.98f;
    settings.post.contrast = 1.10f;
    settings.post.saturation = 0.98f;
    settings.post.bloomThreshold = 0.46f;
    settings.post.bloomIntensity = 0.28f;
    settings.post.bloomRadius = 2.10f;
    settings.post.vignetteStrength = 0.34f;
    settings.post.vignetteSoftness = 0.58f;
    settings.post.splitToneStrength = 0.24f;
    settings.post.splitToneBalance = 0.44f;
    settings.post.shadowTint = glm::vec3(0.64f, 0.50f, 0.30f);
    settings.post.highlightTint = glm::vec3(0.96f, 0.78f, 0.40f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.20f, 0.14f, 0.08f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.07f, 0.05f, 0.03f);
    settings.lighting.hemisphereStrength = 0.24f;
    settings.lighting.enableDirectionalLights = false;
    settings.lighting.directionalIntensityScale = 0.0f;
    return settings;
}

EnvironmentRenderSettings makeSunlitMeadowSettings() {
    EnvironmentRenderSettings settings = makeNeutralSettings();
    settings.profile = EnvironmentProfile::SunlitMeadow;
    settings.post.paletteVariant = 2;
    settings.post.fogNearColor = glm::vec3(0.66f, 0.76f, 0.84f);
    settings.post.fogFarColor = glm::vec3(0.86f, 0.89f, 0.82f);
    settings.post.fogDensity = 0.028f;
    settings.post.fogStart = 18.0f;
    settings.post.exposure = 1.12f;
    settings.post.gamma = 1.0f;
    settings.post.contrast = 1.04f;
    settings.post.saturation = 1.10f;
    settings.post.bloomThreshold = 0.68f;
    settings.post.bloomIntensity = 0.16f;
    settings.post.bloomRadius = 1.45f;
    settings.post.vignetteStrength = 0.16f;
    settings.post.vignetteSoftness = 0.70f;
    settings.post.splitToneStrength = 0.12f;
    settings.post.splitToneBalance = 0.58f;
    settings.post.shadowTint = glm::vec3(0.82f, 0.90f, 0.96f);
    settings.post.highlightTint = glm::vec3(1.00f, 0.92f, 0.66f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.56f, 0.72f, 0.92f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.26f, 0.31f, 0.15f);
    settings.lighting.hemisphereStrength = 0.52f;
    settings.lighting.enableDirectionalLights = true;
    settings.lighting.directionalIntensityScale = 1.24f;
    return settings;
}

EnvironmentRenderSettings makeMountainDuskSettings() {
    EnvironmentRenderSettings settings = makeNeutralSettings();
    settings.profile = EnvironmentProfile::MountainDusk;
    settings.post.paletteVariant = 3;
    settings.post.fogNearColor = glm::vec3(0.24f, 0.24f, 0.34f);
    settings.post.fogFarColor = glm::vec3(0.56f, 0.54f, 0.70f);
    settings.post.fogDensity = 0.038f;
    settings.post.fogStart = 15.0f;
    settings.post.exposure = 0.96f;
    settings.post.gamma = 1.0f;
    settings.post.contrast = 1.02f;
    settings.post.saturation = 0.92f;
    settings.post.bloomThreshold = 0.64f;
    settings.post.bloomIntensity = 0.14f;
    settings.post.bloomRadius = 1.60f;
    settings.post.vignetteStrength = 0.22f;
    settings.post.vignetteSoftness = 0.68f;
    settings.post.splitToneStrength = 0.18f;
    settings.post.splitToneBalance = 0.42f;
    settings.post.shadowTint = glm::vec3(0.58f, 0.60f, 0.82f);
    settings.post.highlightTint = glm::vec3(0.90f, 0.82f, 0.64f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.34f, 0.36f, 0.54f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.10f, 0.12f, 0.16f);
    settings.lighting.hemisphereStrength = 0.38f;
    settings.lighting.enableDirectionalLights = true;
    settings.lighting.directionalIntensityScale = 0.72f;
    return settings;
}

EnvironmentRenderSettings makeArcaneFieldSettings() {
    EnvironmentRenderSettings settings = makeNeutralSettings();
    settings.profile = EnvironmentProfile::ArcaneField;
    settings.post.paletteVariant = 4;
    settings.post.fogNearColor = glm::vec3(0.18f, 0.30f, 0.30f);
    settings.post.fogFarColor = glm::vec3(0.74f, 0.68f, 0.42f);
    settings.post.fogDensity = 0.030f;
    settings.post.fogStart = 16.0f;
    settings.post.exposure = 1.08f;
    settings.post.gamma = 0.98f;
    settings.post.contrast = 1.08f;
    settings.post.saturation = 1.12f;
    settings.post.bloomThreshold = 0.56f;
    settings.post.bloomIntensity = 0.20f;
    settings.post.bloomRadius = 1.90f;
    settings.post.vignetteStrength = 0.18f;
    settings.post.vignetteSoftness = 0.76f;
    settings.post.splitToneStrength = 0.22f;
    settings.post.splitToneBalance = 0.50f;
    settings.post.shadowTint = glm::vec3(0.64f, 0.78f, 0.76f);
    settings.post.highlightTint = glm::vec3(0.98f, 0.84f, 0.42f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.22f, 0.46f, 0.52f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.30f, 0.24f, 0.08f);
    settings.lighting.hemisphereStrength = 0.46f;
    settings.lighting.enableDirectionalLights = true;
    settings.lighting.directionalIntensityScale = 1.06f;
    return settings;
}

EnvironmentRenderSettings makeCathedralArcadeSettings() {
    EnvironmentRenderSettings settings = makeNeutralSettings();
    settings.profile = EnvironmentProfile::CathedralArcade;
    settings.post.paletteVariant = 5;
    settings.post.thresholdBias = -0.018f;
    settings.post.fogNearColor = glm::vec3(0.06f, 0.07f, 0.07f);
    settings.post.fogFarColor = glm::vec3(0.15f, 0.17f, 0.15f);
    settings.post.fogDensity = 0.034f;
    settings.post.fogStart = 14.5f;
    settings.post.exposure = 1.04f;
    settings.post.gamma = 1.0f;
    settings.post.contrast = 1.06f;
    settings.post.saturation = 0.90f;
    settings.post.bloomThreshold = 0.54f;
    settings.post.bloomIntensity = 0.10f;
    settings.post.bloomRadius = 1.50f;
    settings.post.vignetteStrength = 0.22f;
    settings.post.vignetteSoftness = 0.64f;
    settings.post.splitToneStrength = 0.04f;
    settings.post.splitToneBalance = 0.50f;
    settings.post.shadowTint = glm::vec3(0.82f, 0.88f, 0.84f);
    settings.post.highlightTint = glm::vec3(0.94f, 0.89f, 0.78f);

    settings.lighting.hemisphereSkyColor = glm::vec3(0.18f, 0.21f, 0.18f);
    settings.lighting.hemisphereGroundColor = glm::vec3(0.05f, 0.06f, 0.04f);
    settings.lighting.hemisphereStrength = 0.40f;
    settings.lighting.enableDirectionalLights = false;
    settings.lighting.directionalIntensityScale = 0.0f;
    return settings;
}

} // namespace

bool tryParseEnvironmentProfileToken(const std::string& token, EnvironmentProfile& profile) {
    if (token == "neutral") {
        profile = EnvironmentProfile::Neutral;
        return true;
    }
    if (token == "dungeon_torch") {
        profile = EnvironmentProfile::DungeonTorch;
        return true;
    }
    if (token == "sunlit_meadow") {
        profile = EnvironmentProfile::SunlitMeadow;
        return true;
    }
    if (token == "mountain_dusk") {
        profile = EnvironmentProfile::MountainDusk;
        return true;
    }
    if (token == "arcane_field") {
        profile = EnvironmentProfile::ArcaneField;
        return true;
    }
    if (token == "cathedral_arcade") {
        profile = EnvironmentProfile::CathedralArcade;
        return true;
    }
    return false;
}

const char* environmentProfileName(EnvironmentProfile profile) {
    switch (profile) {
    case EnvironmentProfile::Neutral:
        return "neutral";
    case EnvironmentProfile::DungeonTorch:
        return "dungeon_torch";
    case EnvironmentProfile::SunlitMeadow:
        return "sunlit_meadow";
    case EnvironmentProfile::MountainDusk:
        return "mountain_dusk";
    case EnvironmentProfile::ArcaneField:
        return "arcane_field";
    case EnvironmentProfile::CathedralArcade:
        return "cathedral_arcade";
    }
    return "neutral";
}

EnvironmentRenderSettings makeEnvironmentRenderSettings(EnvironmentProfile profile) {
    switch (profile) {
    case EnvironmentProfile::DungeonTorch:
        return makeDungeonTorchSettings();
    case EnvironmentProfile::SunlitMeadow:
        return makeSunlitMeadowSettings();
    case EnvironmentProfile::MountainDusk:
        return makeMountainDuskSettings();
    case EnvironmentProfile::ArcaneField:
        return makeArcaneFieldSettings();
    case EnvironmentProfile::CathedralArcade:
        return makeCathedralArcadeSettings();
    case EnvironmentProfile::Neutral:
    default:
        return makeNeutralSettings();
    }
}
