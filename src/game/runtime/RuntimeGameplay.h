#pragma once

#include "engine/ecs/GameRegistry.h"

class ContentRegistry;
class InputSystem;
class PhysicsSystem;
struct RunSession;

struct RuntimeInventoryCaptureState {
    bool openedByMenu = false;
};

void initializeRuntimeInteraction(GameRegistry& registry);
void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input);

void initializeRuntimeInventory(GameRegistry& registry);
void updateRuntimeInventory(GameRegistry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content);

void updateRuntimePlayerMovement(GameRegistry& registry,
                                 const InputSystem& input,
                                 PhysicsSystem& physics,
                                 float deltaTime);

void updateRuntimeCamera(GameRegistry& registry,
                         const InputSystem& input,
                         float aspect,
                         float deltaTime);

void updateRuntimeBehaviors(GameRegistry& registry);
