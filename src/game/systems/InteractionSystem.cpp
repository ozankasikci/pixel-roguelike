#include "game/systems/InteractionSystem.h"

#include "engine/core/Application.h"
#include "game/runtime/RuntimeGameplay.h"
#include "engine/input/InputSystem.h"

InteractionSystem::InteractionSystem(InputSystem& input)
    : input_(input)
{}

void InteractionSystem::init(Application& app) {
    initializeRuntimeInteraction(app.registry());
}

void InteractionSystem::update(Application& app, float deltaTime) {
    (void)deltaTime;
    updateRuntimeInteraction(app.registry(), input_);
}

void InteractionSystem::shutdown() {
}
