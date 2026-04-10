#pragma once

#include "engine/ecs/GameRegistry.h"

struct ActionEntry;

void handleLockPlayerAction(GameRegistry& registry,
                            entt::entity source,
                            entt::entity target,
                            ActionEntry& action);

void handleUnlockPlayerAction(GameRegistry& registry,
                              entt::entity source,
                              entt::entity target,
                              ActionEntry& action);

void handleTeleportPlayerAction(GameRegistry& registry,
                                entt::entity source,
                                entt::entity target,
                                ActionEntry& action);
