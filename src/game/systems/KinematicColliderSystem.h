#pragma once

#include "engine/ecs/GameRegistry.h"
#include "engine/core/System.h"

class PhysicsSystem;

// Free function for contexts without Application& (e.g. RuntimeGameSession)
void tickKinematicColliders(GameRegistry& registry, PhysicsSystem& physics, float deltaTime);

// System wrapper for the Application framework
class KinematicColliderSystem : public System {
public:
    explicit KinematicColliderSystem(PhysicsSystem& physics);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    PhysicsSystem& physics_;
};
