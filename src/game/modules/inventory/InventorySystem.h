#pragma once

#include "engine/ecs/GameRegistry.h"

class ContentRegistry;
class InputSystem;
struct RunSession;

void initializeRuntimeInventory(GameRegistry& registry);
void updateRuntimeInventory(GameRegistry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content);
void resetRuntimeInventory(GameRegistry& registry);
void clearRuntimeInventory(GameRegistry& registry);
