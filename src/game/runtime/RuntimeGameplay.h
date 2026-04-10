#pragma once

#include "engine/ecs/GameRegistry.h"

class ContentRegistry;
class InputSystem;
struct RunSession;

struct RuntimeInventoryCaptureState {
    bool openedByMenu = false;
};

void initializeRuntimeInventory(GameRegistry& registry);
void updateRuntimeInventory(GameRegistry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content);
