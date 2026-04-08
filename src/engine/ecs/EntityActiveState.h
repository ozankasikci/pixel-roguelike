#pragma once

#include "engine/ecs/GameRegistry.h"

// Set an entity active or inactive, propagating through the hierarchy.
// When disabling: adds DisabledTag to entity and all descendants.
// When enabling: removes DisabledTag from entity and descendants,
//   unless a descendant has activeSelf == false (explicitly disabled).
void setEntityActive(GameRegistry& registry, entt::entity entity, bool active);

// Returns the entity's explicit active state (true if never explicitly disabled).
bool isActiveSelf(const GameRegistry& registry, entt::entity entity);

// Returns whether the entity is actually active in the hierarchy
// (no DisabledTag present).
bool isActiveInHierarchy(const GameRegistry& registry, entt::entity entity);
