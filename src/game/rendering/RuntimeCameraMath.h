#pragma once

#include <glm/glm.hpp>

#include <entt/entt.hpp>

struct CameraComponent;
struct TransformComponent;

struct RuntimeCameraState {
    glm::vec3 position{0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float moveSpeed = 3.0f;
};

void updateRuntimeCameraComponent(TransformComponent& transform,
                                  CameraComponent& camera,
                                  float aspect);

RuntimeCameraState capturePrimaryRuntimeCamera(entt::registry& registry,
                                               float aspect);
