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
