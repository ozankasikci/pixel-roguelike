#include "game/modules/player_control/PlayerControlMovement.h"

#include "engine/input/InputSystem.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>

namespace {

glm::vec3 moveTowardXZ(const glm::vec3& current, const glm::vec3& target, float maxDelta) {
    glm::vec2 currentXZ(current.x, current.z);
    glm::vec2 targetXZ(target.x, target.z);
    const glm::vec2 delta = targetXZ - currentXZ;
    const float distance = glm::length(delta);

    if (distance <= maxDelta || distance < 0.0001f) {
        return glm::vec3(targetXZ.x, current.y, targetXZ.y);
    }

    const glm::vec2 result = currentXZ + (delta / distance) * maxDelta;
    return glm::vec3(result.x, current.y, result.y);
}

} // namespace

void tickPlayerMovement(GameRegistry& registry,
                        const InputSystem& input,
                        PhysicsSystem& physics,
                        float deltaTime) {
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    auto view = registry.view<TransformComponent,
                              CameraComponent,
                              PlayerMovementComponent,
                              CharacterControllerComponent,
                              PlayerInteractionLockComponent,
                              PlayerSpawnComponent,
                              PlayerTag,
                              ControllableTag,
                              PrimaryCameraTag>();
    for (auto [entity, transform, camera, movement, controller, lock, spawn] : view.each()) {
        const bool cursorLocked = input.isCursorLocked();
        const bool locked = lock.active;
        const bool gameplayInputEnabled = cursorLocked && !locked && !inventoryOpen;

        if (transform.position.y < spawn.fallRespawnY) {
            physics.setCharacterVelocity(entity, glm::vec3(0.0f));
            physics.setCharacterPosition(entity,
                                         spawn.respawnPosition - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));
            movement.velocity = glm::vec3(0.0f);
            movement.jumpHeld = false;
            transform.position = spawn.respawnPosition;
            continue;
        }

        glm::vec3 inputDirection(0.0f);
        if (gameplayInputEnabled) {
            const float yawRadians = glm::radians(camera.yaw);
            glm::vec3 forward(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
            forward = glm::normalize(forward);
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (input.isKeyPressed(GLFW_KEY_W)) inputDirection += forward;
            if (input.isKeyPressed(GLFW_KEY_S)) inputDirection -= forward;
            if (input.isKeyPressed(GLFW_KEY_D)) inputDirection += right;
            if (input.isKeyPressed(GLFW_KEY_A)) inputDirection -= right;

            if (glm::length(inputDirection) > 0.001f) {
                inputDirection = glm::normalize(inputDirection);
            }
        }

        const glm::vec3 desiredVelocity = inputDirection * movement.maxGroundSpeed;
        const bool hasInput = glm::length(inputDirection) > 0.001f;
        const GroundState groundState = physics.getCharacterGroundState(entity);
        movement.grounded = (groundState == GroundState::OnGround);

        float acceleration = movement.airAcceleration;
        if (movement.grounded) {
            acceleration = hasInput ? movement.acceleration : movement.deceleration;
        }

        movement.velocity = moveTowardXZ(movement.velocity,
                                         desiredVelocity,
                                         acceleration * deltaTime);

        if (gameplayInputEnabled && movement.grounded && input.isKeyJustPressed(GLFW_KEY_SPACE)) {
            movement.velocity.y = movement.jumpImpulse;
            movement.jumpHeld = true;
            movement.jumpHoldTimer = 0.0f;
        }

        float effectiveGravity = movement.gravity;
        if (movement.jumpHeld && gameplayInputEnabled && input.isKeyPressed(GLFW_KEY_SPACE)
            && movement.velocity.y > 0.0f
            && movement.jumpHoldTimer < movement.maxJumpHoldTime) {
            effectiveGravity = movement.gravity * movement.jumpHoldGravityScale;
            movement.jumpHoldTimer += deltaTime;
        } else {
            movement.jumpHeld = false;
        }

        if (locked || inventoryOpen) {
            movement.jumpHeld = false;
            movement.velocity.x = 0.0f;
            movement.velocity.z = 0.0f;
        }

        movement.velocity.y += effectiveGravity * deltaTime;
        if (movement.grounded && movement.velocity.y < 0.0f) {
            movement.velocity.y = 0.0f;
        }

        physics.setCharacterVelocity(entity, movement.velocity);
        physics.updateCharacter(entity, deltaTime, glm::vec3(0.0f, movement.gravity, 0.0f));

        const glm::vec3 characterPosition = physics.getCharacterPosition(entity);
        transform.position = characterPosition + glm::vec3(0.0f, controller.eyeOffset(), 0.0f);
        movement.grounded = (physics.getCharacterGroundState(entity) == GroundState::OnGround);
        break;
    }
}
