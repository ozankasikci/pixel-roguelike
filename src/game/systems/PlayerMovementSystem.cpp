#include "game/systems/PlayerMovementSystem.h"

#include "engine/core/Application.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/runtime/RuntimeInputState.h"

PlayerMovementSystem::PlayerMovementSystem(RuntimeInputState& input, PhysicsSystem& physics)
    : input_(input), physics_(physics)
{}

void PlayerMovementSystem::init(Application& app) {
    (void)app;
}

void PlayerMovementSystem::update(Application& app, float deltaTime) {
    updateRuntimePlayerMovement(app.registry(), input_, physics_, deltaTime);
}

void PlayerMovementSystem::shutdown() {
}
