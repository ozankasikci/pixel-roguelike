#include "game/systems/DoorSystem.h"

#include "engine/core/Application.h"
#include "game/runtime/RuntimeGameplay.h"
#include "engine/input/InputSystem.h"

DoorSystem::DoorSystem(InputSystem& input)
    : input_(input)
{}

void DoorSystem::init(Application& app) {
    initializeRuntimeDoors(app.registry());
}

void DoorSystem::update(Application& app, float deltaTime) {
    updateRuntimeDoors(app.registry(), deltaTime);
}

void DoorSystem::shutdown() {
}
