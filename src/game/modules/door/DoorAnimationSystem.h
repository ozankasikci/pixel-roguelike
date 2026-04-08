#pragma once

#include "engine/core/System.h"
#include <entt/entt.hpp>

class DoorAnimationSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;
};

// Free function for contexts without Application& (e.g. RuntimeGameSession)
void tickDoorAnimation(entt::registry& registry, float deltaTime);

// Reset all doors to closed state and sync leaf visuals.
// Call after restoring baseline state to ensure PivotTransformComponent.currentYawDeg matches DoorStateComponent.
void resetDoorVisuals(entt::registry& registry);
