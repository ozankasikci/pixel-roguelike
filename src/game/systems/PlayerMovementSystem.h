#pragma once
#include "engine/core/System.h"

class PhysicsSystem;
struct RuntimeInputState;

class PlayerMovementSystem : public System {
public:
    PlayerMovementSystem(RuntimeInputState& input, PhysicsSystem& physics);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    RuntimeInputState& input_;
    PhysicsSystem& physics_;
};
