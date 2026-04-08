#pragma once

#include <entt/entt.hpp>

struct ActionEntry;

void handleCheckpointAction(entt::registry& registry,
                            entt::entity source,
                            entt::entity target,
                            ActionEntry& action);
