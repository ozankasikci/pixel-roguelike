#pragma once

#include "engine/ecs/GameRegistry.h"

// Tick door open/close animations each frame.
void tickDoorAnimation(GameRegistry& registry, float deltaTime);

// Reset all doors to closed state and sync leaf visuals.
// Call after restoring baseline state to ensure modelOverride matches DoorStateComponent.
void resetDoorVisuals(GameRegistry& registry);
