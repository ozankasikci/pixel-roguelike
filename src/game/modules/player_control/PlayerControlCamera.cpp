#include "game/modules/player_control/PlayerControlCamera.h"

#include "engine/camera/CameraManager.h"
#include "engine/camera/CameraMath.h"
#include "engine/input/InputSystem.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InventoryMenuState.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

glm::vec3 buildPlayerCameraForward(float yawDegrees, float pitchDegrees) {
    return buildCameraForward(yawDegrees, pitchDegrees);
}

namespace {

struct PlayerCameraInput {
    glm::vec3 position{0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool found = false;
};

PlayerCameraInput gatherPlayerCameraInput(GameRegistry& registry, const InputSystem& input,
                                          CameraManager& cameraManager) {
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    PlayerCameraInput result;

    auto view = registry.view<TransformComponent,
                              PlayerInteractionLockComponent,
                              ControllableTag,
                              PlayerTag>();
    for (auto [entity, transform, lock] : view.each()) {
        (void)entity;

        const auto& base = cameraManager.getBaseState();
        float yaw = base.yaw;
        float pitch = base.pitch;
        const float fov = base.fov;
        const float nearPlane = base.nearPlane;
        const float farPlane = base.farPlane;

        if (input.isCursorLocked() && !lock.active && !inventoryOpen) {
            constexpr float sensitivity = 0.1f;
            const glm::vec2 delta = input.mouseDelta();
            yaw += delta.x * sensitivity;
            pitch -= delta.y * sensitivity;
        }

        pitch = std::clamp(pitch, -89.0f, 89.0f);

        result.position = transform.position;
        result.yaw = yaw;
        result.pitch = pitch;
        result.fov = fov;
        result.nearPlane = nearPlane;
        result.farPlane = farPlane;
        result.found = true;
        break;
    }

    return result;
}

} // namespace

void tickPlayerCamera(GameRegistry& registry,
                      const InputSystem& input,
                      CameraManager& cameraManager,
                      float aspect,
                      float deltaTime) {
    (void)deltaTime;

    const PlayerCameraInput cam = gatherPlayerCameraInput(registry, input, cameraManager);
    if (!cam.found) {
        return;
    }

    cameraManager.setBaseState(cam.position, cam.yaw, cam.pitch);
    cameraManager.setProjection(cam.fov, aspect, cam.nearPlane, cam.farPlane);
}
