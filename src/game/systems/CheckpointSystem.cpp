#include "game/systems/CheckpointSystem.h"

#include "engine/core/Application.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/session/RunSession.h"

void CheckpointSystem::init(Application& app) {
    initializeRuntimeCheckpoints(app.registry());
}

void CheckpointSystem::update(Application& app, float deltaTime) {
    updateRuntimeCheckpoints(app.registry(), deltaTime, app.getService<RunSession>());
}

void CheckpointSystem::shutdown() {
}
