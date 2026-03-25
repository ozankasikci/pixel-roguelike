#pragma once

#include "engine/core/System.h"

class InputSystem;

class CheckpointSystem : public System {
public:
    explicit CheckpointSystem(InputSystem& input);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    InputSystem& input_;
    float messageTimer_ = 0.0f;
};
