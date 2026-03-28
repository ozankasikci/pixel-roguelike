#pragma once

#include "game/runtime/RuntimeInputState.h"

#include <entt/entity/fwd.hpp>

class ContentRegistry;
class PhysicsSystem;
struct RunSession;

struct RuntimeInventoryCaptureState {
    bool openedByMenu = false;
};

struct RuntimeCheckpointFeedbackState {
    float messageTimer = 0.0f;
};

void initializeRuntimeInteraction(entt::registry& registry);
void updateRuntimeInteraction(entt::registry& registry, const RuntimeInputState& input);

void initializeRuntimeInventory(entt::registry& registry);
void updateRuntimeInventory(entt::registry& registry,
                            RuntimeInputState& input,
                            RunSession& session,
                            const ContentRegistry& content);

void initializeRuntimeDoors(entt::registry& registry);
void updateRuntimeDoors(entt::registry& registry, float deltaTime);

void initializeRuntimeCheckpoints(entt::registry& registry);
void updateRuntimeCheckpoints(entt::registry& registry, float deltaTime, RunSession& session);

void updateRuntimePlayerMovement(entt::registry& registry,
                                 const RuntimeInputState& input,
                                 PhysicsSystem& physics,
                                 float deltaTime);

void updateRuntimeCamera(entt::registry& registry,
                         const RuntimeInputState& input,
                         float aspect,
                         float deltaTime);
