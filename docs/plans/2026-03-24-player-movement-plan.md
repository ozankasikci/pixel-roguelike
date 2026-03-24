# Player Movement System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the free-flying camera with ground-based first-person movement using Jolt Physics CharacterVirtual, with smooth/inertial acceleration, variable-height jumping, step climbing, and wall collision.

**Architecture:** Jolt Physics is integrated behind a pimpl-based `PhysicsSystem` (no Jolt headers leak to game code). Static collision bodies are described via `StaticColliderComponent` on entities and created by PhysicsSystem during init. A `PlayerMovementSystem` handles velocity computation, jumping, and character updates through PhysicsSystem's public API. CameraSystem is simplified to mouse-look only.

**Tech Stack:** Jolt Physics v5.4.0, C++20, EnTT ECS, GLM, GLFW

**Design doc:** `docs/plans/2026-03-24-player-movement-design.md`

---

## Task 1: Add Jolt Physics to CMake

**Files:**
- Modify: `CMakeLists.txt`

**Step 1: Add Jolt Physics FetchContent and engine_physics library**

Add after the existing `FetchContent_MakeAvailable(entt)` block (after line 27):

```cmake
# Jolt Physics v5.4.0 - Rigid body physics, collision, character controller
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.4.0
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  Build
)
FetchContent_MakeAvailable(JoltPhysics)
```

Add the `engine_physics` library after the `engine_input` library block (after line 98):

```cmake
add_library(engine_physics STATIC
    src/engine/physics/PhysicsSystem.cpp
)
target_include_directories(engine_physics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_physics PUBLIC engine_core glm::glm)
target_link_libraries(engine_physics PRIVATE Jolt)
```

Note: Jolt is linked **PRIVATE** so game code doesn't need Jolt headers.

Add `engine_physics` to the `game` library's link dependencies (modify the existing `target_link_libraries(game ...)` line):

```cmake
target_link_libraries(game PUBLIC engine_core engine_rendering engine_ui engine_input engine_physics EnTT::EnTT)
```

Add `engine_physics` to the executable's link dependencies (modify the existing `target_link_libraries(pixel-roguelike PRIVATE ...)` block):

```cmake
target_link_libraries(pixel-roguelike PRIVATE
    engine_core
    engine_rendering
    engine_ui
    engine_scene
    engine_input
    engine_physics
    game
    EnTT::EnTT
)
```

**Step 2: Create placeholder PhysicsSystem files so CMake configures**

Create empty placeholder files (these will be filled in Task 3):

`src/engine/physics/PhysicsSystem.h`:
```cpp
#pragma once
#include "engine/core/System.h"

class PhysicsSystem : public System {
public:
    void init(Application& app) override {}
    void update(Application& app, float deltaTime) override {}
    void shutdown() override {}
};
```

`src/engine/physics/PhysicsSystem.cpp`:
```cpp
#include "engine/physics/PhysicsSystem.h"
```

**Step 3: Verify CMake configures and project compiles**

Run:
```bash
cd build && cmake .. && cmake --build . 2>&1 | tail -5
```
Expected: Jolt fetched and compiled. Project builds successfully. No errors.

**Step 4: Commit**

```bash
git add CMakeLists.txt src/engine/physics/
git commit -m "Add Jolt Physics v5.4.0 dependency and engine_physics library"
```

---

## Task 2: Create New ECS Components

**Files:**
- Create: `src/game/components/StaticColliderComponent.h`
- Create: `src/game/components/CharacterControllerComponent.h`
- Create: `src/game/components/PlayerMovementComponent.h`

**Step 1: Create StaticColliderComponent**

`src/game/components/StaticColliderComponent.h`:
```cpp
#pragma once
#include <glm/glm.hpp>

enum class ColliderShape { Box, Cylinder };

struct StaticColliderComponent {
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 position{0.0f};       // world-space center of the collider
    glm::vec3 halfExtents{0.0f};    // for Box: half-sizes on each axis
    float radius = 0.0f;            // for Cylinder
    float halfHeight = 0.0f;        // for Cylinder
};
```

**Step 2: Create CharacterControllerComponent**

`src/game/components/CharacterControllerComponent.h`:
```cpp
#pragma once

struct CharacterControllerComponent {
    float eyeHeight = 1.6f;          // eye position above capsule bottom
    float capsuleRadius = 0.2f;      // capsule cap radius
    float capsuleHalfHeight = 0.7f;  // capsule cylinder half-height (total height = 2*(halfHeight+radius) = 1.8m)

    // Derived: offset from capsule center to eye position
    float eyeOffset() const {
        return eyeHeight - capsuleHalfHeight - capsuleRadius;
    }
};
```

**Step 3: Create PlayerMovementComponent**

`src/game/components/PlayerMovementComponent.h`:
```cpp
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
```

**Step 4: Verify project compiles**

Run:
```bash
cd build && cmake --build . 2>&1 | tail -5
```
Expected: Build succeeds (components are header-only, not yet included anywhere).

**Step 5: Commit**

```bash
git add src/game/components/StaticColliderComponent.h src/game/components/CharacterControllerComponent.h src/game/components/PlayerMovementComponent.h
git commit -m "Add ECS components for physics colliders, character controller, and player movement"
```

---

## Task 3: Create PhysicsSystem

The PhysicsSystem initializes Jolt, creates static collision bodies from `StaticColliderComponent` entities, creates a `CharacterVirtual` from `CharacterControllerComponent`, and steps the physics world each frame. All Jolt types are hidden behind a pimpl.

**Files:**
- Modify: `src/engine/physics/PhysicsSystem.h` (replace placeholder)
- Modify: `src/engine/physics/PhysicsSystem.cpp` (replace placeholder)

**Step 1: Write PhysicsSystem.h (public API, no Jolt headers)**

Replace `src/engine/physics/PhysicsSystem.h` with:

```cpp
#pragma once
#include "engine/core/System.h"
#include <glm/glm.hpp>
#include <memory>

enum class GroundState { OnGround, OnSteepGround, InAir };

class PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    // Non-copyable
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

    // Character controller API (used by PlayerMovementSystem)
    void setCharacterVelocity(const glm::vec3& velocity);
    void updateCharacter(float deltaTime, const glm::vec3& gravity);
    glm::vec3 getCharacterPosition() const;
    GroundState getCharacterGroundState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

**Step 2: Write PhysicsSystem.cpp (full Jolt implementation)**

Replace `src/engine/physics/PhysicsSystem.cpp` with:

```cpp
#include "engine/physics/PhysicsSystem.h"
#include "engine/core/Application.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/TransformComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <spdlog/spdlog.h>
#include <vector>

// --- Jolt type conversions ---
static inline JPH::Vec3 toJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
static inline JPH::RVec3 toJoltR(const glm::vec3& v) { return JPH::RVec3(v.x, v.y, v.z); }
static inline glm::vec3 toGlm(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
static inline glm::vec3 toGlm(const JPH::RVec3& v) { return glm::vec3(float(v.GetX()), float(v.GetY()), float(v.GetZ())); }

// --- Jolt layer definitions ---
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::uint NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr unsigned int NUM_LAYERS = 2;
}

// --- BroadPhaseLayerInterface ---
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        switch (inLayer) {
            case Layers::NON_MOVING: return BroadPhaseLayers::NON_MOVING;
            case Layers::MOVING:     return BroadPhaseLayers::MOVING;
            default:                 return BroadPhaseLayers::NON_MOVING;
        }
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif
};

// --- ObjectVsBroadPhaseLayerFilter ---
class OVBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// --- ObjectLayerPairFilter ---
class OLPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// --- PhysicsSystem::Impl ---
struct PhysicsSystem::Impl {
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;

    BPLayerInterfaceImpl bpLayerInterface;
    OVBPLayerFilterImpl objectVsBPFilter;
    OLPairFilterImpl objectLayerPairFilter;

    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    // Character controller
    JPH::CharacterVirtual* character = nullptr;
    float characterEyeOffset = 0.7f;

    // Tracked bodies for cleanup
    std::vector<JPH::BodyID> staticBodies;

    // Fixed timestep accumulator
    float accumulator = 0.0f;
    static constexpr float fixedTimeStep = 1.0f / 60.0f;
};

// --- PhysicsSystem implementation ---

PhysicsSystem::PhysicsSystem() = default;
PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::init(Application& app) {
    // 1. Initialize Jolt runtime
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // 2. Create implementation
    impl_ = std::make_unique<Impl>();
    impl_->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024); // 10 MB
    impl_->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1); // 1 worker thread

    // 3. Create Jolt PhysicsSystem
    impl_->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    impl_->physicsSystem->Init(
        1024,   // max bodies
        0,      // num body mutexes (0 = auto)
        1024,   // max body pairs
        1024,   // max contact constraints
        impl_->bpLayerInterface,
        impl_->objectVsBPFilter,
        impl_->objectLayerPairFilter
    );

    auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
    auto& registry = app.registry();

    // 4. Create static bodies from StaticColliderComponent entities
    auto colliderView = registry.view<StaticColliderComponent>();
    for (auto [entity, collider] : colliderView.each()) {
        JPH::ShapeRefC shape;

        if (collider.shape == ColliderShape::Box) {
            shape = new JPH::BoxShape(toJolt(collider.halfExtents));
        } else if (collider.shape == ColliderShape::Cylinder) {
            shape = new JPH::CylinderShape(collider.halfHeight, collider.radius);
        }

        if (shape) {
            JPH::BodyCreationSettings bodySettings(
                shape,
                toJoltR(collider.position),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
            impl_->staticBodies.push_back(bodyId);
        }
    }

    spdlog::info("PhysicsSystem: created {} static bodies", impl_->staticBodies.size());

    // 5. Create CharacterVirtual from CharacterControllerComponent
    auto ccView = registry.view<CharacterControllerComponent, TransformComponent>();
    for (auto [entity, cc, transform] : ccView.each()) {
        // Derive capsule center from eye position
        float eyeOff = cc.eyeOffset();
        glm::vec3 capsuleCenter = transform.position - glm::vec3(0.0f, eyeOff, 0.0f);

        // Create capsule shape (standing upright)
        JPH::RefConst<JPH::Shape> capsuleShape = new JPH::CapsuleShape(cc.capsuleHalfHeight, cc.capsuleRadius);

        JPH::CharacterVirtualSettings settings;
        settings.mShape = capsuleShape;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mMass = 80.0f;
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cc.capsuleRadius);

        impl_->character = new JPH::CharacterVirtual(
            &settings,
            toJoltR(capsuleCenter),
            JPH::Quat::sIdentity(),
            0,  // user data
            impl_->physicsSystem.get()
        );

        impl_->characterEyeOffset = eyeOff;

        spdlog::info("PhysicsSystem: created CharacterVirtual at ({:.1f}, {:.1f}, {:.1f})",
            capsuleCenter.x, capsuleCenter.y, capsuleCenter.z);
        break; // only one player
    }

    // 6. Optimize broad phase after all bodies added
    impl_->physicsSystem->OptimizeBroadPhase();

    spdlog::info("PhysicsSystem initialized");
}

void PhysicsSystem::update(Application& app, float deltaTime) {
    if (!impl_ || !impl_->physicsSystem) return;

    // Fixed timestep stepping (for future dynamic bodies)
    impl_->accumulator += deltaTime;
    while (impl_->accumulator >= Impl::fixedTimeStep) {
        impl_->physicsSystem->Update(
            Impl::fixedTimeStep,
            1, // collision steps
            impl_->tempAllocator.get(),
            impl_->jobSystem.get()
        );
        impl_->accumulator -= Impl::fixedTimeStep;
    }
}

void PhysicsSystem::shutdown() {
    if (!impl_) return;

    // Destroy character
    if (impl_->character) {
        delete impl_->character;
        impl_->character = nullptr;
    }

    // Remove static bodies
    if (impl_->physicsSystem) {
        auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
        for (auto& bodyId : impl_->staticBodies) {
            bodyInterface.RemoveBody(bodyId);
            bodyInterface.DestroyBody(bodyId);
        }
        impl_->staticBodies.clear();
    }

    impl_.reset();

    // Cleanup Jolt runtime
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    spdlog::info("PhysicsSystem shutdown");
}

// --- Character controller API ---

void PhysicsSystem::setCharacterVelocity(const glm::vec3& velocity) {
    if (impl_ && impl_->character) {
        impl_->character->SetLinearVelocity(toJolt(velocity));
    }
}

void PhysicsSystem::updateCharacter(float deltaTime, const glm::vec3& gravity) {
    if (!impl_ || !impl_->character) return;

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.3f, 0);

    impl_->character->ExtendedUpdate(
        deltaTime,
        toJolt(gravity),
        updateSettings,
        impl_->physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        impl_->physicsSystem->GetDefaultLayerFilter(Layers::MOVING),
        {},  // BodyFilter (accept all)
        {},  // ShapeFilter (accept all)
        *impl_->tempAllocator
    );
}

glm::vec3 PhysicsSystem::getCharacterPosition() const {
    if (impl_ && impl_->character) {
        return toGlm(impl_->character->GetPosition());
    }
    return glm::vec3(0.0f);
}

GroundState PhysicsSystem::getCharacterGroundState() const {
    if (!impl_ || !impl_->character) return GroundState::InAir;

    switch (impl_->character->GetGroundState()) {
        case JPH::CharacterVirtual::EGroundState::OnGround:
            return GroundState::OnGround;
        case JPH::CharacterVirtual::EGroundState::OnSteepGround:
            return GroundState::OnSteepGround;
        default:
            return GroundState::InAir;
    }
}
```

**Step 3: Verify project compiles**

Run:
```bash
cd build && cmake --build . 2>&1 | tail -10
```
Expected: Build succeeds. PhysicsSystem compiles with Jolt.

Note: If there are Jolt API differences in v5.4.0 (method signatures, includes), check the Jolt headers at `build/_deps/joltphysics-src/Jolt/` and adjust. The structure and approach are correct; exact signatures may need minor tweaks.

**Step 4: Commit**

```bash
git add src/engine/physics/PhysicsSystem.h src/engine/physics/PhysicsSystem.cpp
git commit -m "Implement PhysicsSystem with Jolt world, static bodies, and CharacterVirtual"
```

---

## Task 4: Create PlayerMovementSystem

Handles WASD input, velocity computation with smooth acceleration/deceleration, variable-height jumping, and CharacterVirtual updates via PhysicsSystem's public API.

**Files:**
- Create: `src/game/systems/PlayerMovementSystem.h`
- Create: `src/game/systems/PlayerMovementSystem.cpp`
- Modify: `CMakeLists.txt` (add .cpp to `game` library)

**Step 1: Write PlayerMovementSystem.h**

`src/game/systems/PlayerMovementSystem.h`:
```cpp
#pragma once
#include "engine/core/System.h"

class InputSystem;
class PhysicsSystem;

class PlayerMovementSystem : public System {
public:
    PlayerMovementSystem(InputSystem& input, PhysicsSystem& physics);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    InputSystem& input_;
    PhysicsSystem& physics_;
};
```

**Step 2: Write PlayerMovementSystem.cpp**

`src/game/systems/PlayerMovementSystem.cpp`:
```cpp
#include "game/systems/PlayerMovementSystem.h"
#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/CharacterControllerComponent.h"
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
    if (input_.wantsCaptureMouse()) return;

    auto& registry = app.registry();

    // Find the player entity (has all three components)
    auto view = registry.view<TransformComponent, CameraComponent, PlayerMovementComponent, CharacterControllerComponent>();
    for (auto [entity, transform, cam, movement, cc] : view.each()) {

        // --- 1. Compute movement direction from camera yaw (horizontal only) ---
        float yawRad = glm::radians(cam.yaw);
        glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
        forward = glm::normalize(forward);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        // --- 2. Read WASD input ---
        glm::vec3 inputDir(0.0f);
        if (input_.isKeyPressed(GLFW_KEY_W)) inputDir += forward;
        if (input_.isKeyPressed(GLFW_KEY_S)) inputDir -= forward;
        if (input_.isKeyPressed(GLFW_KEY_D)) inputDir += right;
        if (input_.isKeyPressed(GLFW_KEY_A)) inputDir -= right;

        // Normalize input direction (prevent diagonal speed boost)
        if (glm::length(inputDir) > 0.001f) {
            inputDir = glm::normalize(inputDir);
        }

        // --- 3. Compute desired velocity ---
        glm::vec3 desiredVelocity = inputDir * movement.maxGroundSpeed;

        // --- 4. Apply acceleration/deceleration ---
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

        // --- 5. Handle jumping ---
        if (movement.grounded && input_.isKeyJustPressed(GLFW_KEY_SPACE)) {
            // Start jump
            movement.velocity.y = movement.jumpImpulse;
            movement.jumpHeld = true;
            movement.jumpHoldTimer = 0.0f;
        }

        // Apply gravity (with variable jump hold)
        float effectiveGravity = movement.gravity;
        if (movement.jumpHeld && input_.isKeyPressed(GLFW_KEY_SPACE)
            && movement.velocity.y > 0.0f
            && movement.jumpHoldTimer < movement.maxJumpHoldTime) {
            // Reduced gravity while holding jump and rising
            effectiveGravity = movement.gravity * movement.jumpHoldGravityScale;
            movement.jumpHoldTimer += deltaTime;
        } else {
            movement.jumpHeld = false;
        }

        movement.velocity.y += effectiveGravity * deltaTime;

        // Reset Y velocity when grounded and not jumping
        if (movement.grounded && movement.velocity.y < 0.0f) {
            movement.velocity.y = 0.0f;
        }

        // --- 6. Update physics character ---
        physics_.setCharacterVelocity(movement.velocity);
        physics_.updateCharacter(deltaTime, glm::vec3(0.0f, movement.gravity, 0.0f));

        // --- 7. Sync transform from physics ---
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
```

**Step 3: Add PlayerMovementSystem.cpp to CMake game library**

In `CMakeLists.txt`, add the file to the `game` library sources:

```cmake
add_library(game STATIC
    src/game/scenes/CathedralScene.cpp
    src/game/systems/CameraSystem.cpp
    src/game/systems/PlayerMovementSystem.cpp
    src/game/systems/RenderSystem.cpp
)
```

**Step 4: Verify project compiles**

Run:
```bash
cd build && cmake .. && cmake --build . 2>&1 | tail -5
```
Expected: Build succeeds.

**Step 5: Commit**

```bash
git add src/game/systems/PlayerMovementSystem.h src/game/systems/PlayerMovementSystem.cpp CMakeLists.txt
git commit -m "Add PlayerMovementSystem with inertial movement and variable-height jumping"
```

---

## Task 5: Modify CameraSystem — Remove WASD Movement

CameraSystem should only handle mouse look and compute view/projection matrices. WASD movement is now handled by PlayerMovementSystem.

**Files:**
- Modify: `src/game/systems/CameraSystem.cpp`

**Step 1: Remove WASD movement block**

In `src/game/systems/CameraSystem.cpp`, delete lines 48-60 (the WASD movement block):

```cpp
        // DELETE THIS ENTIRE BLOCK:
        // WASD movement
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
```

The CameraSystem should now only contain: mouse look, pitch clamp, direction computation, vector storage, and view/projection matrix computation.

**Step 2: Verify project compiles**

Run:
```bash
cd build && cmake --build . 2>&1 | tail -5
```
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add src/game/systems/CameraSystem.cpp
git commit -m "Remove WASD movement from CameraSystem (now handled by PlayerMovementSystem)"
```

---

## Task 6: Modify CathedralScene — Add Collision Shapes and Player Physics

Add `StaticColliderComponent` to entities for collision, and add `CharacterControllerComponent` + `PlayerMovementComponent` to the camera entity.

**Files:**
- Modify: `src/game/scenes/CathedralScene.cpp`

**Step 1: Add includes**

Add to the top of `CathedralScene.cpp`, after the existing includes:

```cpp
#include "game/components/StaticColliderComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/PlayerMovementComponent.h"
```

**Step 2: Add collision entity helper functions**

Add after the existing `addArch` helper function (after line 52), before `CathedralScene::onEnter`:

```cpp
static entt::entity addColliderBox(entt::registry& registry,
                                   std::vector<entt::entity>& entities,
                                   glm::vec3 position, glm::vec3 halfExtents) {
    auto e = registry.create();
    StaticColliderComponent sc;
    sc.shape = ColliderShape::Box;
    sc.position = position;
    sc.halfExtents = halfExtents;
    registry.emplace<StaticColliderComponent>(e, sc);
    entities.push_back(e);
    return e;
}

static entt::entity addColliderCylinder(entt::registry& registry,
                                        std::vector<entt::entity>& entities,
                                        glm::vec3 position, float radius, float halfHeight) {
    auto e = registry.create();
    StaticColliderComponent sc;
    sc.shape = ColliderShape::Cylinder;
    sc.position = position;
    sc.radius = radius;
    sc.halfHeight = halfHeight;
    registry.emplace<StaticColliderComponent>(e, sc);
    entities.push_back(e);
    return e;
}
```

**Step 3: Add collision shapes after visual geometry**

Add the following collision setup at the end of `onEnter`, just before the Camera section (before line 182). Place it after the entrance light block:

```cpp
    // ===== Collision geometry =====

    // Floor collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, -0.05f, -corridorLength * 0.5f),
        glm::vec3(halfW, 0.05f, corridorLength * 0.5f));

    // Ceiling collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight + 0.05f, -corridorLength * 0.5f),
        glm::vec3(halfW, 0.05f, corridorLength * 0.5f));

    // Left wall collider
    addColliderBox(registry, entities_,
        glm::vec3(-halfW, wallHeight * 0.5f, -corridorLength * 0.5f),
        glm::vec3(0.15f, wallHeight * 0.5f, corridorLength * 0.5f));

    // Right wall collider
    addColliderBox(registry, entities_,
        glm::vec3(halfW, wallHeight * 0.5f, -corridorLength * 0.5f),
        glm::vec3(0.15f, wallHeight * 0.5f, corridorLength * 0.5f));

    // Back wall collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight * 0.5f, -corridorLength),
        glm::vec3(halfW, wallHeight * 0.5f, 0.15f));

    // Front wall (invisible, prevents walking out the entrance)
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight * 0.5f, 1.0f),
        glm::vec3(halfW, wallHeight * 0.5f, 0.15f));

    // Pillar colliders
    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;
        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);
            addColliderCylinder(registry, entities_,
                glm::vec3(x, wallHeight * 0.5f, z),
                pillarRadius, wallHeight * 0.5f);
        }
    }

    // Step colliders
    for (int s = 0; s < 4; ++s) {
        float sz = -corridorLength + 2.0f + s * 0.6f;
        float sy = s * 0.15f;
        addColliderBox(registry, entities_,
            glm::vec3(0.0f, sy + 0.075f, sz),
            glm::vec3(corridorWidth * 0.3f, 0.075f, 0.3f));
    }
```

**Step 4: Modify camera entity to include physics components**

Replace the Camera section (lines 182-187) with:

```cpp
    // Camera / Player
    {
        auto e = registry.create();
        // Spawn at eye height — PhysicsSystem derives capsule center from this
        registry.emplace<TransformComponent>(e, TransformComponent{glm::vec3(0.0f, 1.6f, 4.0f)});
        registry.emplace<CameraComponent>(e);
        registry.emplace<CharacterControllerComponent>(e);
        registry.emplace<PlayerMovementComponent>(e);
        entities_.push_back(e);
    }
```

**Step 5: Verify project compiles**

Run:
```bash
cd build && cmake --build . 2>&1 | tail -5
```
Expected: Build succeeds.

**Step 6: Commit**

```bash
git add src/game/scenes/CathedralScene.cpp
git commit -m "Add collision shapes and player physics components to cathedral scene"
```

---

## Task 7: Wire Systems in main.cpp

Register `PhysicsSystem` and `PlayerMovementSystem` in the correct pipeline order.

**Files:**
- Modify: `src/main.cpp`

**Step 1: Add includes**

Add to the top of `main.cpp`:

```cpp
#include "engine/physics/PhysicsSystem.h"
#include "game/systems/PlayerMovementSystem.h"
```

**Step 2: Register systems in correct order**

Replace the system registration block (lines 27-29):

```cpp
    // Register systems (execution order: Input → Physics → Movement → Camera → Render)
    auto& input    = app.addSystem<InputSystem>();
    auto& physics  = app.addSystem<PhysicsSystem>();
    auto& movement = app.addSystem<PlayerMovementSystem>(input, physics);
    auto& camera   = app.addSystem<CameraSystem>(input);
    auto& render   = app.addSystem<RenderSystem>();
```

**Step 3: Verify project compiles and runs**

Run:
```bash
cd build && cmake .. && cmake --build . && ./pixel-roguelike
```
Expected: Game launches. Player stands on the cathedral floor. WASD moves with smooth acceleration. Space jumps. Cannot walk through walls or pillars. Steps at the back of the cathedral can be climbed.

**Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "Wire PhysicsSystem and PlayerMovementSystem into game loop"
```

---

## Task 8: Add ImGui Movement Debug Panel

Expose all movement tuning parameters in ImGui for runtime tweaking.

**Files:**
- Modify: `src/engine/ui/ImGuiLayer.h`
- Modify: `src/engine/ui/ImGuiLayer.cpp`
- Modify: `src/game/systems/RenderSystem.cpp`
- Modify: `src/game/systems/RenderSystem.h`

**Step 1: Add renderMovementOverlay declaration to ImGuiLayer.h**

Add to `ImGuiLayer` class in `ImGuiLayer.h`, after the existing `renderOverlay` declaration:

```cpp
    static void renderMovementOverlay(struct PlayerMovementComponent& movement, bool grounded);
```

Also add a forward declaration at the top of the file (after the existing forward declarations):

```cpp
struct PlayerMovementComponent;
```

**Step 2: Implement renderMovementOverlay in ImGuiLayer.cpp**

Add at the end of `ImGuiLayer.cpp`, before the closing of the file:

```cpp
#include "game/components/PlayerMovementComponent.h"

void ImGuiLayer::renderMovementOverlay(PlayerMovementComponent& movement, bool grounded) {
    ImGui::Begin("Movement");

    // Ground state
    const char* stateStr = grounded ? "ON GROUND" : "IN AIR";
    ImGui::Text("State: %s", stateStr);
    ImGui::Text("Velocity: %.1f, %.1f, %.1f",
        movement.velocity.x, movement.velocity.y, movement.velocity.z);
    ImGui::Text("Speed: %.1f", glm::length(glm::vec2(movement.velocity.x, movement.velocity.z)));

    ImGui::Separator();

    // Ground movement
    if (ImGui::CollapsingHeader("Ground Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Max Speed", &movement.maxGroundSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Acceleration", &movement.acceleration, 5.0f, 60.0f);
        ImGui::SliderFloat("Deceleration", &movement.deceleration, 5.0f, 60.0f);
    }

    // Air movement
    if (ImGui::CollapsingHeader("Air Movement")) {
        ImGui::SliderFloat("Air Accel", &movement.airAcceleration, 1.0f, 30.0f);
    }

    // Gravity & jump
    if (ImGui::CollapsingHeader("Jump & Gravity")) {
        ImGui::SliderFloat("Gravity", &movement.gravity, -30.0f, -5.0f);
        ImGui::SliderFloat("Jump Impulse", &movement.jumpImpulse, 2.0f, 12.0f);
        ImGui::SliderFloat("Hold Gravity Scale", &movement.jumpHoldGravityScale, 0.1f, 1.0f);
        ImGui::SliderFloat("Max Hold Time", &movement.maxJumpHoldTime, 0.1f, 0.6f);
    }

    ImGui::End();
}
```

**Step 3: Call renderMovementOverlay from RenderSystem**

In `RenderSystem.cpp`, add the include at the top:

```cpp
#include "game/components/PlayerMovementComponent.h"
```

In `RenderSystem::update()`, add after the existing `ImGuiLayer::renderOverlay(debugParams_, lights);` line:

```cpp
    // Movement debug panel
    {
        auto movView = registry.view<PlayerMovementComponent>();
        for (auto [entity, movement] : movView.each()) {
            ImGuiLayer::renderMovementOverlay(movement, movement.grounded);
            break;
        }
    }
```

**Step 4: Add glm include to ImGuiLayer.cpp**

Since `renderMovementOverlay` uses `glm::length` and `glm::vec2`, add at the top of the new function or with the existing includes in `ImGuiLayer.cpp`:

The `PlayerMovementComponent.h` already includes `<glm/glm.hpp>`, which provides `glm::length` and `glm::vec2`. No additional include needed.

**Step 5: Verify project compiles and runs with debug panel**

Run:
```bash
cd build && cmake --build . && ./pixel-roguelike
```
Expected: Game runs. A "Movement" ImGui window appears showing ground state, velocity, and sliders for all tuning parameters. Changing slider values immediately affects movement feel.

**Step 6: Commit**

```bash
git add src/engine/ui/ImGuiLayer.h src/engine/ui/ImGuiLayer.cpp src/game/systems/RenderSystem.cpp
git commit -m "Add ImGui movement debug panel with runtime-tunable parameters"
```

---

## Verification Checklist

After all tasks are complete, verify the following:

1. **Gravity works**: Player falls when stepping off edges (e.g., off the steps)
2. **Ground walking**: Player walks on the floor without sinking or floating
3. **Wall collision**: Player cannot walk through side walls or back wall
4. **Pillar collision**: Player slides around pillars, cannot pass through
5. **Step climbing**: Player walks up the 4 steps at the back of the cathedral without jumping
6. **Smooth acceleration**: Player accelerates gradually when pressing WASD, decelerates when releasing
7. **Variable jump**: Tapping space gives a short hop; holding space gives a full jump
8. **Air control**: Player can steer slightly while airborne, but less than on ground
9. **Mouse look**: Looking up/down does not affect movement speed (movement is horizontal-only)
10. **ImGui tuning**: All movement sliders work and affect gameplay in real-time
