#include "game/rendering/EnvironmentProfile.h"
#include "common/TestSupport.h"

#include <cassert>

int main() {
    const auto settings = makeEnvironmentRenderSettings(EnvironmentProfile::Default);
    assert(settings.sky.enabled);
    assert(!settings.sky.cloudLayerAPath.empty());
    assert(settings.lighting.enableDirectionalLights);
    assert(settings.lighting.sun.enabled);
    assert(settings.lighting.fill.enabled);
    assert(settings.lighting.sun.intensity > settings.lighting.fill.intensity);
    assert(settings.post.toneMapMode == 1);
    assert(settings.post.paletteVariant == 0);
    assert(!settings.post.enableEdges);
    assert(!settings.post.enableGrain);
    assert(test_support::nearlyEqualVec3(settings.lighting.sun.direction, settings.sky.sunDirection));

    const auto defaultSettings = makeDefaultEnvironmentRenderSettings();
    assert(defaultSettings.sky.enabled);
    assert(test_support::nearlyEqual(defaultSettings.post.exposure, settings.post.exposure));

    return 0;
}
