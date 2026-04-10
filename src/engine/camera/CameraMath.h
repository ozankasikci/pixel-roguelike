#pragma once

#include "engine/camera/CameraState.h"

#include <glm/glm.hpp>

float evaluateEasing(EasingType type, float t);

glm::vec3 buildCameraForward(float yawDegrees, float pitchDegrees);

void rebuildCameraVectors(CameraState& state);

void rebuildCameraMatrices(CameraState& state, float aspectRatio);
