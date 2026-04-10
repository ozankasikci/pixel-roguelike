#include "game/rendering/EnvironmentProfile.h"
#include "common/TestSupport.h"

#include <cassert>
#include <string>

int main() {
    // Default profile settings
    const auto settings = makeEnvironmentRenderSettings(EnvironmentProfile::Default);
    assert(settings.post.enableSky);
    assert(!settings.sky.cloudLayerAPath.empty());
    assert(settings.lighting.enableDirectionalLights);
    assert(settings.lighting.sun.enabled);
    assert(settings.lighting.fill.enabled);
    assert(settings.lighting.sun.intensity > settings.lighting.fill.intensity);
    assert(settings.post.toneMapMode == 1);
    assert(!settings.post.enableEdges);
    assert(settings.post.enableFxaa);
    assert(!settings.post.enableGrain);
    assert(!settings.post.ssaoHalfResolution);
    assert(settings.post.exposure >= 0.95f);
    assert(settings.post.bloomIntensity < 0.15f);
    assert(settings.post.ssaoFadeEnd > settings.post.ssaoFadeStart);
    assert(settings.sky.sunSize > 0.0f);

    const auto defaultSettings = makeDefaultEnvironmentRenderSettings();
    assert(defaultSettings.post.enableSky);
    assert(test_support::nearlyEqual(defaultSettings.post.exposure, settings.post.exposure));

    // environmentProfileName round-trip
    assert(std::string(environmentProfileName(EnvironmentProfile::Default)) == "default");

    // tryParseEnvironmentProfileToken: "default" token
    EnvironmentProfile parsed = EnvironmentProfile::Default;
    assert(tryParseEnvironmentProfileToken("default", parsed));
    assert(parsed == EnvironmentProfile::Default);

    // Backward compatibility: all old profile tokens map to Default
    const char* oldTokens[] = {
        "game_ready_neutral",
        "neutral",
        "dungeon_torch",
        "sunlit_meadow",
        "mountain_dusk",
        "arcane_field",
        "cathedral_arcade",
        "cloister_daylight",
    };
    for (const char* token : oldTokens) {
        EnvironmentProfile result = EnvironmentProfile::Default;
        assert(tryParseEnvironmentProfileToken(token, result));
        assert(result == EnvironmentProfile::Default);
    }

    // Unknown tokens return false
    EnvironmentProfile unknown = EnvironmentProfile::Default;
    assert(!tryParseEnvironmentProfileToken("nonexistent_profile", unknown));
    assert(!tryParseEnvironmentProfileToken("", unknown));

    return 0;
}
