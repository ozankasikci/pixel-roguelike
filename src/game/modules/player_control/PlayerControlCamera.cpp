#include "game/modules/player_control/PlayerControlCamera.h"

#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/rendering/RuntimeCameraMath.h"
#include "game/ui/InventoryMenuState.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

glm::vec3 buildPlayerCameraForward(float yawDegrees, float pitchDegrees) {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(yawDegrees)) * std::cos(glm::radians(pitchDegrees));
    forward.y = std::sin(glm::radians(pitchDegrees));
    forward.z = std::sin(glm::radians(yawDegrees)) * std::cos(glm::radians(pitchDegrees));

    if (glm::dot(forward, forward) <= 0.0001f) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }

    return glm::normalize(forward);
}

void tickPlayerCamera(GameRegistry& registry,
                      const InputSystem& input,
                      float aspect,
                      float deltaTime) {
    (void)deltaTime;

    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    auto view = registry.view<TransformComponent,
                              CameraComponent,
                              PlayerInteractionLockComponent,
                              ControllableTag,
                              PrimaryCameraTag>();
    for (auto [entity, transform, camera, lock] : view.each()) {
        (void)entity;

        if (input.isCursorLocked() && !lock.active && !inventoryOpen) {
            constexpr float sensitivity = 0.1f;
            const glm::vec2 delta = input.mouseDelta();
            camera.yaw += delta.x * sensitivity;
            camera.pitch -= delta.y * sensitivity;
        }

        camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
        updateRuntimeCameraComponent(transform, camera, aspect);
        break;
    }
}
