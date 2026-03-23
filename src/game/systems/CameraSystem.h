#pragma once
#include "engine/core/System.h"

class InputSystem;

class CameraSystem : public System {
public:
    explicit CameraSystem(InputSystem& input);
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    InputSystem& input_;
};
