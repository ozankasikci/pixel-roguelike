#pragma once

#include "engine/ecs/GameRegistry.h"

void initializeCheckpointFeedback(GameRegistry& registry);
void tickCheckpointFeedback(GameRegistry& registry, float deltaTime);
