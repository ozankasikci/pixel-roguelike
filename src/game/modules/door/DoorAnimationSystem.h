#pragma once

#include "engine/core/System.h"
#include "engine/ecs/GameRegistry.h"

class DoorAnimationSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;
};

// Free function for contexts without Application& (e.g. RuntimeGameSession)
void tickDoorAnimation(GameRegistry& registry, float deltaTime);

// Reset all doors to closed state and sync leaf visuals.
// Call after restoring baseline state to ensure modelOverride matches DoorStateComponent.
void resetDoorVisuals(GameRegistry& registry);
