#include "game/level/LevelDef.h"

#include <cassert>

int main() {
    const auto data = loadLevelDef(LIGHT_RECORDS_SCENE_FILE);

    assert(data.environmentProfile == EnvironmentProfile::SunlitMeadow);
    assert(data.lights.size() == 3);

    const auto& point = data.lights[0];
    assert(point.type == LightType::Point);
    assert(point.position == glm::vec3(1.0f, 2.0f, 3.0f));
    assert(point.color == glm::vec3(0.8f, 0.7f, 0.6f));
    assert(point.radius == 6.5f);
    assert(point.intensity == 1.2f);
    assert(!point.castsShadows);

    const auto& spot = data.lights[1];
    assert(spot.type == LightType::Spot);
    assert(spot.position == glm::vec3(4.0f, 5.0f, 6.0f));
    assert(spot.direction == glm::vec3(0.0f, -1.0f, 0.0f));
    assert(spot.color == glm::vec3(1.0f, 0.9f, 0.8f));
    assert(spot.radius == 10.0f);
    assert(spot.intensity == 2.4f);
    assert(spot.innerConeDegrees == 18.0f);
    assert(spot.outerConeDegrees == 30.0f);
    assert(spot.castsShadows);

    const auto& directional = data.lights[2];
    assert(directional.type == LightType::Directional);
    assert(directional.direction == glm::vec3(-0.2f, -1.0f, 0.1f));
    assert(directional.color == glm::vec3(0.7f, 0.75f, 0.8f));
    assert(directional.intensity == 0.65f);
    assert(directional.radius == 0.0f);

    return 0;
}
