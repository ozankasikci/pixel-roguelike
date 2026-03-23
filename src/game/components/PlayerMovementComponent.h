#pragma once
#include <glm/glm.hpp>

struct PlayerMovementComponent {
    // Current state
    glm::vec3 velocity{0.0f};
    bool grounded = false;

    // Ground movement tuning
    float maxGroundSpeed = 5.0f;
    float acceleration = 25.0f;       // reach full speed in ~0.2s
    float deceleration = 20.0f;       // stop in ~0.25s

    // Air movement tuning
    float airAcceleration = 8.0f;     // ~30-40% of ground accel

    // Gravity
    float gravity = -15.0f;

    // Jump tuning
    float jumpImpulse = 6.0f;
    float jumpHoldGravityScale = 0.4f;
    float maxJumpHoldTime = 0.3f;

    // Jump state (runtime)
    float jumpHoldTimer = 0.0f;
    bool jumpHeld = false;
};
