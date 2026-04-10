#pragma once

#include "engine/ecs/GameRegistry.h"

#include <glm/glm.hpp>

class CameraManager;
class InputSystem;

glm::vec3 buildPlayerCameraForward(float yawDegrees, float pitchDegrees);

void tickPlayerCamera(GameRegistry& registry,
                      const InputSystem& input,
                      CameraManager& cameraManager,
                      float aspect,
                      float deltaTime);
