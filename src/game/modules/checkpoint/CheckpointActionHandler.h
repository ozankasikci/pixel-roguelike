#pragma once

#include "engine/ecs/GameRegistry.h"

struct ActionEntry;

void handleCheckpointAction(GameRegistry& registry,
                            entt::entity source,
                            entt::entity target,
                            ActionEntry& action);
