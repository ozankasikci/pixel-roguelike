#include "engine/camera/CameraMath.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

float evaluateEasing(EasingType type, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
        case EasingType::Linear:
            return t;
        case EasingType::EaseOutQuad:
            return t * (2.0f - t);
        case EasingType::EaseOutCubic:
            return 1.0f - std::pow(1.0f - t, 3.0f);
        case EasingType::EaseInOutCubic:
            return t < 0.5f
                ? 4.0f * t * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        case EasingType::EaseInOutQuad:
            return t < 0.5f
                ? 2.0f * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    return t;
}

glm::vec3 buildCameraForward(float yawDegrees, float pitchDegrees) {
    const float yawRad = glm::radians(yawDegrees);
    const float pitchRad = glm::radians(pitchDegrees);
    glm::vec3 forward;
    forward.x = std::cos(yawRad) * std::cos(pitchRad);
    forward.y = std::sin(pitchRad);
    forward.z = std::sin(yawRad) * std::cos(pitchRad);
    if (glm::dot(forward, forward) <= 0.0001f) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return glm::normalize(forward);
}

void rebuildCameraVectors(CameraState& state) {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    state.forward = buildCameraForward(state.yaw, state.pitch);
    state.right = glm::normalize(glm::cross(state.forward, worldUp));
    state.up = glm::normalize(glm::cross(state.right, state.forward));
}

void rebuildCameraMatrices(CameraState& state, float aspectRatio) {
    rebuildCameraVectors(state);
    state.viewMatrix = glm::lookAt(state.position,
                                   state.position + state.forward,
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    state.projectionMatrix = glm::perspective(glm::radians(state.fov),
                                              std::max(aspectRatio, 0.0001f),
                                              state.nearPlane,
                                              state.farPlane);
}
