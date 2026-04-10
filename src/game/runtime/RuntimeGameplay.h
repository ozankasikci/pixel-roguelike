#pragma once

#include "engine/ecs/GameRegistry.h"

class ContentRegistry;
class InputSystem;
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
