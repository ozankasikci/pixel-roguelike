#include "game/systems/CameraSystem.h"
#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/rendering/RuntimeCameraMath.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

CameraSystem::CameraSystem(InputSystem& input)
    : input_(input)
{}

void CameraSystem::init(Application& app) {
    (void)app;
}

void CameraSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    auto view = registry.view<TransformComponent, CameraComponent, PlayerInteractionLockComponent, ControllableTag, PrimaryCameraTag>();
    for (auto [entity, transform, cam, lock] : view.each()) {

        // Mouse look (only when cursor is locked)
        if (input_.isCursorLocked() && !lock.active && !inventoryOpen) {
            constexpr float sensitivity = 0.1f;
            glm::vec2 delta = input_.mouseDelta();
            cam.yaw   += delta.x * sensitivity;
            cam.pitch -= delta.y * sensitivity;  // inverted: moving mouse up = look up
        }

        // Clamp pitch
        if (cam.pitch >  89.0f) cam.pitch =  89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        float aspect = static_cast<float>(app.window().width()) /
                       static_cast<float>(app.window().height());
        updateRuntimeCameraComponent(transform, cam, aspect);

        break;
    }
}

void CameraSystem::shutdown() {
}
