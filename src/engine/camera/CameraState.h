#pragma once

#include <glm/glm.hpp>

enum class EasingType {
    Linear,
    EaseOutQuad,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInOutQuad
};

struct CameraState {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};
