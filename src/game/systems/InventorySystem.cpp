#include "game/systems/InventorySystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/content/ContentRegistry.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/session/RunSession.h"

InventorySystem::InventorySystem(InputSystem& input)
    : input_(input) {}

void InventorySystem::init(Application& app) {
    initializeRuntimeInventory(app.registry());
}

void InventorySystem::update(Application& app, float deltaTime) {
    (void)deltaTime;
    updateRuntimeInventory(app.registry(),
                           input_,
                           app.getService<RunSession>(),
                           app.getService<ContentRegistry>());
}

void InventorySystem::shutdown() {}
