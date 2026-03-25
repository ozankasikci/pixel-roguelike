#include "game/systems/PlayerMovementSystem.h"
#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/TransformComponent.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cmath>

// Smoothly move current toward target by at most maxDelta (on XZ plane, preserving Y)
static glm::vec3 moveTowardXZ(const glm::vec3& current, const glm::vec3& target, float maxDelta) {
    glm::vec2 cur(current.x, current.z);
    glm::vec2 tgt(target.x, target.z);
    glm::vec2 diff = tgt - cur;
    float dist = glm::length(diff);

    if (dist <= maxDelta || dist < 0.0001f) {
        return glm::vec3(tgt.x, current.y, tgt.y);
    }

    glm::vec2 result = cur + (diff / dist) * maxDelta;
    return glm::vec3(result.x, current.y, result.y);
}

PlayerMovementSystem::PlayerMovementSystem(InputSystem& input, PhysicsSystem& physics)
    : input_(input), physics_(physics)
{}

void PlayerMovementSystem::init(Application& app) {
    (void)app;
}

void PlayerMovementSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();

    // Find the player entity (has all three components)
    auto view = registry.view<TransformComponent, CameraComponent, PlayerMovementComponent, CharacterControllerComponent, PlayerInteractionLockComponent, PlayerSpawnComponent>();
    for (auto [entity, transform, cam, movement, cc, lock, spawn] : view.each()) {

        bool captured = input_.wantsCaptureMouse();
        const bool locked = lock.active;

        if (transform.position.y < spawn.fallRespawnY) {
            physics_.setCharacterVelocity(glm::vec3(0.0f));
            physics_.setCharacterPosition(spawn.respawnPosition - glm::vec3(0.0f, cc.eyeOffset(), 0.0f));
            movement.velocity = glm::vec3(0.0f);
            movement.jumpHeld = false;
            transform.position = spawn.respawnPosition;
            continue;
        }

        // --- 1. Compute movement direction and read input ---
        glm::vec3 inputDir(0.0f);

        if (!captured && !locked) {
            // Compute movement direction from camera yaw (horizontal only)
            float yawRad = glm::radians(cam.yaw);
            glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
            forward = glm::normalize(forward);
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            // Read WASD input
            if (input_.isKeyPressed(GLFW_KEY_W)) inputDir += forward;
            if (input_.isKeyPressed(GLFW_KEY_S)) inputDir -= forward;
            if (input_.isKeyPressed(GLFW_KEY_D)) inputDir += right;
            if (input_.isKeyPressed(GLFW_KEY_A)) inputDir -= right;

            // Normalize input direction (prevent diagonal speed boost)
            if (glm::length(inputDir) > 0.001f) {
                inputDir = glm::normalize(inputDir);
            }
        }
        // When captured, inputDir stays zero — no movement input

        // --- 2. Compute desired velocity ---
        glm::vec3 desiredVelocity = inputDir * movement.maxGroundSpeed;

        // --- 3. Apply acceleration/deceleration ---
        bool hasInput = glm::length(inputDir) > 0.001f;
        GroundState groundState = physics_.getCharacterGroundState();
        movement.grounded = (groundState == GroundState::OnGround);

        float accel;
        if (movement.grounded) {
            accel = hasInput ? movement.acceleration : movement.deceleration;
        } else {
            accel = movement.airAcceleration;
        }

        movement.velocity = moveTowardXZ(movement.velocity, desiredVelocity, accel * deltaTime);

        // --- 4. Handle jumping (only allow new jumps when not captured) ---
        if (!captured && !locked && movement.grounded && input_.isKeyJustPressed(GLFW_KEY_SPACE)) {
            // Start jump
            movement.velocity.y = movement.jumpImpulse;
            movement.jumpHeld = true;
            movement.jumpHoldTimer = 0.0f;
        }

        // Apply gravity (with variable jump hold)
        float effectiveGravity = movement.gravity;
        if (movement.jumpHeld && !captured && !locked && input_.isKeyPressed(GLFW_KEY_SPACE)
            && movement.velocity.y > 0.0f
            && movement.jumpHoldTimer < movement.maxJumpHoldTime) {
            // Reduced gravity while holding jump and rising
            effectiveGravity = movement.gravity * movement.jumpHoldGravityScale;
            movement.jumpHoldTimer += deltaTime;
        } else {
            movement.jumpHeld = false;
        }

        if (locked) {
            movement.jumpHeld = false;
            movement.velocity.x = 0.0f;
            movement.velocity.z = 0.0f;
        }

        movement.velocity.y += effectiveGravity * deltaTime;

        // Reset Y velocity when grounded and not jumping
        if (movement.grounded && movement.velocity.y < 0.0f) {
            movement.velocity.y = 0.0f;
        }

        // --- 5. Update physics character ---
        physics_.setCharacterVelocity(movement.velocity);
        physics_.updateCharacter(deltaTime, glm::vec3(0.0f, movement.gravity, 0.0f));

        // --- 6. Sync transform from physics ---
        glm::vec3 charPos = physics_.getCharacterPosition();
        transform.position = charPos + glm::vec3(0.0f, cc.eyeOffset(), 0.0f);

        // Update velocity from character (physics may have modified it due to collisions)
        // Keep our velocity for next frame's acceleration math

        // Refresh ground state after physics update
        movement.grounded = (physics_.getCharacterGroundState() == GroundState::OnGround);

        break; // only one player
    }
}

void PlayerMovementSystem::shutdown() {
}
