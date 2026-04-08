#pragma once

#include <entt/entt.hpp>

void initializeCheckpointFeedback(entt::registry& registry);
void tickCheckpointFeedback(entt::registry& registry, float deltaTime);
