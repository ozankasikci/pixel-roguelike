#pragma once

#include <entt/entt.hpp>

class PhysicsSystem;

void tickKinematicColliders(entt::registry& registry, PhysicsSystem& physics);
