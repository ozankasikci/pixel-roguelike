#include "game/rendering/RuntimeCameraMath.h"

#include "game/components/CameraComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace {

glm::vec3 buildForward(float yawDegrees, float pitchDegrees) {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(yawDegrees)) * std::cos(glm::radians(pitchDegrees));
    forward.y = std::sin(glm::radians(pitchDegrees));
    forward.z = std::sin(glm::radians(yawDegrees)) * std::cos(glm::radians(pitchDegrees));
    if (glm::dot(forward, forward) <= 0.0001f) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return glm::normalize(forward);
}

} // namespace

void updateRuntimeCameraComponent(TransformComponent& transform,
                                  CameraComponent& camera,
                                  float aspect) {
    const glm::vec3 forward = buildForward(camera.yaw, camera.pitch);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(forward, up));

    camera.forward = forward;
    camera.right = right;
    camera.viewMatrix = glm::lookAt(transform.position, transform.position + forward, up);
    camera.projectionMatrix = glm::perspective(glm::radians(camera.fov),
                                               std::max(aspect, 0.0001f),
                                               camera.nearPlane,
                                               camera.farPlane);
}

RuntimeCameraState capturePrimaryRuntimeCamera(entt::registry& registry,
                                               float aspect) {
    RuntimeCameraState state;

    auto view = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : view.each()) {
        updateRuntimeCameraComponent(transform, camera, aspect);
        state.position = transform.position;
        state.viewMatrix = camera.viewMatrix;
        state.projectionMatrix = camera.projectionMatrix;
        state.direction = camera.forward;
        state.nearPlane = camera.nearPlane;
        state.farPlane = camera.farPlane;
        state.moveSpeed = camera.moveSpeed;
        break;
    }

    return state;
}
