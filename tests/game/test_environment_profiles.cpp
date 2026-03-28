#include "game/rendering/EnvironmentProfile.h"
#include "common/TestSupport.h"

#include <cassert>

int main() {
    const auto neutral = makeEnvironmentRenderSettings(EnvironmentProfile::Neutral);
    assert(neutral.sky.enabled);
    assert(!neutral.sky.cloudLayerAPath.empty());
    assert(neutral.lighting.enableDirectionalLights);
    assert(neutral.lighting.sun.enabled);
    assert(!neutral.lighting.fill.enabled);
    assert(test_support::nearlyEqualVec3(neutral.lighting.sun.direction, neutral.sky.sunDirection));

    const auto dungeon = makeEnvironmentRenderSettings(EnvironmentProfile::DungeonTorch);
    assert(!dungeon.sky.enabled);
    assert(!dungeon.lighting.enableDirectionalLights);
    assert(!dungeon.lighting.sun.enabled);
    assert(!dungeon.lighting.fill.enabled);

    const auto meadow = makeEnvironmentRenderSettings(EnvironmentProfile::SunlitMeadow);
    assert(meadow.sky.enabled);
    assert(!meadow.sky.cloudLayerAPath.empty());
    assert(!meadow.sky.horizonLayerPath.empty());
    assert(meadow.lighting.sun.enabled);
    assert(meadow.lighting.fill.enabled);
    assert(meadow.lighting.sun.intensity > 1.0f);
    assert(meadow.lighting.sun.intensity > meadow.lighting.fill.intensity);
    assert(test_support::nearlyEqualVec3(meadow.lighting.sun.direction, meadow.sky.sunDirection));

    const auto dusk = makeEnvironmentRenderSettings(EnvironmentProfile::MountainDusk);
    assert(dusk.sky.enabled);
    assert(dusk.sky.moonEnabled);
    assert(!dusk.sky.horizonLayerPath.empty());
    assert(dusk.lighting.sun.enabled);
    assert(dusk.lighting.fill.enabled);

    const auto cloister = makeEnvironmentRenderSettings(EnvironmentProfile::CloisterDaylight);
    assert(cloister.sky.enabled);
    assert(!cloister.sky.cubemapFacePaths[0].empty());
    assert(cloister.lighting.sun.enabled);
    assert(cloister.lighting.fill.enabled);
    assert(cloister.lighting.fill.intensity < cloister.lighting.sun.intensity);
    assert(test_support::nearlyEqualVec3(cloister.lighting.sun.direction, cloister.sky.sunDirection));
    assert(cloister.post.fogFarColor.g > cloister.post.fogNearColor.g);

    return 0;
}
