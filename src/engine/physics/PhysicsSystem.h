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
