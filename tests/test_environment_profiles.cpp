#include "game/rendering/EnvironmentProfile.h"

#include <cassert>
#include <cmath>

int main() {
    auto sameDirection = [](const glm::vec3& a, const glm::vec3& b) {
        return std::abs(a.x - b.x) < 0.0001f
            && std::abs(a.y - b.y) < 0.0001f
            && std::abs(a.z - b.z) < 0.0001f;
    };

    const auto neutral = makeEnvironmentRenderSettings(EnvironmentProfile::Neutral);
    assert(neutral.sky.enabled);
    assert(!neutral.sky.cloudLayerAPath.empty());
    assert(neutral.lighting.enableDirectionalLights);
    assert(neutral.lighting.sun.enabled);
    assert(!neutral.lighting.fill.enabled);
    assert(sameDirection(neutral.lighting.sun.direction, neutral.sky.sunDirection));

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
    assert(sameDirection(meadow.lighting.sun.direction, meadow.sky.sunDirection));

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
    assert(sameDirection(cloister.lighting.sun.direction, cloister.sky.sunDirection));
    assert(cloister.post.fogFarColor.g > cloister.post.fogNearColor.g);

    return 0;
}
