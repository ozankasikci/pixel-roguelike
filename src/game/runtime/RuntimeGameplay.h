#pragma once

#include <entt/entity/fwd.hpp>

class ContentRegistry;
class InputSystem;
class PhysicsSystem;
struct RunSession;

struct RuntimeInventoryCaptureState {
    bool openedByMenu = false;
};

struct RuntimeCheckpointFeedbackState {
    float messageTimer = 0.0f;
};

void initializeRuntimeInteraction(entt::registry& registry);
void updateRuntimeInteraction(entt::registry& registry, const InputSystem& input);

void initializeRuntimeInventory(entt::registry& registry);
void updateRuntimeInventory(entt::registry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content);

void initializeRuntimeCheckpoints(entt::registry& registry);
void updateRuntimeCheckpoints(entt::registry& registry, float deltaTime, RunSession& session);

void updateRuntimePlayerMovement(entt::registry& registry,
                                 const InputSystem& input,
                                 PhysicsSystem& physics,
                                 float deltaTime);

void updateRuntimeCamera(entt::registry& registry,
                         const InputSystem& input,
                         float aspect,
                         float deltaTime);

void updateRuntimeBehaviors(entt::registry& registry);
void updateRuntimeDoorAnimation(entt::registry& registry, float deltaTime);
