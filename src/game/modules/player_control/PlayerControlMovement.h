#pragma once

#include "engine/ecs/GameRegistry.h"

class CameraManager;
class InputSystem;
class PhysicsSystem;

void tickPlayerMovement(GameRegistry& registry,
                        const InputSystem& input,
                        const CameraManager& cameraManager,
                        PhysicsSystem& physics,
                        float deltaTime);
