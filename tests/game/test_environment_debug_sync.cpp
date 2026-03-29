#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "common/TestSupport.h"

#include <entt/entt.hpp>

#include <cassert>

int main() {
    entt::registry registry;

    DebugParams params;
    params.post.enableEdges = true;
    params.post.enableGrain = true;
    params.post.debugViewMode = 3;
    params.post.nearPlane = 0.25f;
    params.post.farPlane = 250.0f;

    EnvironmentDefinition definition = makeEnvironmentDefinition(EnvironmentProfile::GameReadyNeutral);
    definition.post.exposure = 1.37f;
    definition.post.enableEdges = false;
    definition.post.enableGrain = false;

    registry.ctx().emplace<RuntimeEnvironmentOverride>(RuntimeEnvironmentOverride{definition});

    syncEnvironmentFromRegistry(registry, params, nullptr, true);

    assert(test_support::nearlyEqual(params.post.exposure, 1.37f));
    assert(!params.post.enableEdges);
    assert(!params.post.enableGrain);
    assert(params.post.debugViewMode == 3);
    assert(test_support::nearlyEqual(params.post.nearPlane, 0.25f));
    assert(test_support::nearlyEqual(params.post.farPlane, 250.0f));

    return 0;
}
