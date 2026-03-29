#include "game/systems/CameraSystem.h"

#include "engine/core/Application.h"
#include "game/runtime/RuntimeGameplay.h"
#include "engine/input/InputSystem.h"

CameraSystem::CameraSystem(InputSystem& input)
    : input_(input)
{}

void CameraSystem::init(Application& app) {
    (void)app;
}

void CameraSystem::update(Application& app, float deltaTime) {
    const float aspect = static_cast<float>(app.window().width()) /
                         static_cast<float>(app.window().height());
    updateRuntimeCamera(app.registry(), input_, aspect, deltaTime);
}

void CameraSystem::shutdown() {
}
