#pragma once

#include "engine/ecs/GameRegistry.h"

class InputSystem;
class PhysicsSystem;

void tickPlayerMovement(GameRegistry& registry,
                        const InputSystem& input,
                        PhysicsSystem& physics,
                        float deltaTime);
