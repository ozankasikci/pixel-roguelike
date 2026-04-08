#pragma once

#include "engine/ecs/GameRegistry.h"

struct ActionEntry;

// Handle door-specific actions (OpenDoor, CloseDoor, ToggleDoor).
// source: the entity that owns the BehaviorComponent that triggered the action.
// target: the resolved target entity (may equal source for "self" targets).
// action: the ActionEntry being executed (mutable for fireOnce state update).
void handleDoorAction(GameRegistry& registry,
                      entt::entity source,
                      entt::entity target,
                      ActionEntry& action);
