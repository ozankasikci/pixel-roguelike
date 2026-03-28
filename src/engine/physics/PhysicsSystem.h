#pragma once
#include "engine/core/System.h"
#include <entt/entity/fwd.hpp>
#include <entt/entt.hpp>
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

    void init(entt::registry& registry);
    void update(entt::registry& registry, float deltaTime);
    void shutdownRuntime();

    // Character controller API (used by gameplay systems)
    void setCharacterVelocity(entt::entity entity, const glm::vec3& velocity);
    void updateCharacter(entt::entity entity, float deltaTime, const glm::vec3& gravity);
    glm::vec3 getCharacterPosition(entt::entity entity) const;
    void setCharacterPosition(entt::entity entity, const glm::vec3& position);
    GroundState getCharacterGroundState(entt::entity entity) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
