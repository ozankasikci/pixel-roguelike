#include "game/systems/CameraSystem.h"
#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/TransformComponent.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

CameraSystem::CameraSystem(InputSystem& input)
    : input_(input)
{}

void CameraSystem::init(Application& app) {
    // Nothing to initialize -- camera entity is created by the scene
    (void)app;
}

void CameraSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();

    // Query for the camera entity (exactly one expected)
    auto view = registry.view<TransformComponent, CameraComponent>();
    for (auto [entity, transform, cam] : view.each()) {
        // Compute initial forward direction from current yaw/pitch
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        forward.y = std::sin(glm::radians(cam.pitch));
        forward.z = std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        forward = glm::normalize(forward);

        glm::vec3 up{0.0f, 1.0f, 0.0f};
        glm::vec3 right = glm::normalize(glm::cross(forward, up));

        // Movement (only when ImGui doesn't own mouse) -- mirrors main.cpp lines 98-111
        if (!input_.wantsCaptureMouse()) {
            float moveSpeed = cam.moveSpeed * deltaTime;

            if (input_.isKeyPressed(GLFW_KEY_W))
                transform.position += forward * moveSpeed;
            if (input_.isKeyPressed(GLFW_KEY_S))
                transform.position -= forward * moveSpeed;
            if (input_.isKeyPressed(GLFW_KEY_A))
                transform.position -= right * moveSpeed;
            if (input_.isKeyPressed(GLFW_KEY_D))
                transform.position += right * moveSpeed;
        }

        // Arrow key look -- mirrors main.cpp lines 113-121
        {
            float lookSpeed = 60.0f * deltaTime;
            if (input_.isKeyPressed(GLFW_KEY_LEFT))  cam.yaw   -= lookSpeed;
            if (input_.isKeyPressed(GLFW_KEY_RIGHT)) cam.yaw   += lookSpeed;
            if (input_.isKeyPressed(GLFW_KEY_UP))    cam.pitch += lookSpeed;
            if (input_.isKeyPressed(GLFW_KEY_DOWN))  cam.pitch -= lookSpeed;
        }

        // Clamp pitch -- mirrors main.cpp lines 123-124
        if (cam.pitch >  89.0f) cam.pitch =  89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        // Recompute forward after yaw/pitch change -- mirrors main.cpp lines 126-130
        forward.x = std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        forward.y = std::sin(glm::radians(cam.pitch));
        forward.z = std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        forward = glm::normalize(forward);

        right = glm::normalize(glm::cross(forward, up));

        // Store computed vectors into CameraComponent
        cam.forward = forward;
        cam.right   = right;

        // Compute view matrix -- mirrors main.cpp lines 132-136
        cam.viewMatrix = glm::lookAt(
            transform.position,
            transform.position + forward,
            up
        );

        // Compute projection matrix -- mirrors main.cpp lines 137-139
        float aspect = static_cast<float>(app.window().width()) /
                       static_cast<float>(app.window().height());
        cam.projectionMatrix = glm::perspective(
            glm::radians(cam.fov), aspect, cam.nearPlane, cam.farPlane
        );

        break;  // only process the first camera entity
    }
}

void CameraSystem::shutdown() {
    // no-op
}
