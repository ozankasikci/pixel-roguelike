#pragma once

#include "engine/ecs/GameRegistry.h"

class PhysicsSystem;

void tickKinematicColliders(GameRegistry& registry, PhysicsSystem& physics);
