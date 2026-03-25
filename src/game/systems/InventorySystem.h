#pragma once

#include "engine/core/System.h"

class InputSystem;

class InventorySystem : public System {
public:
    explicit InventorySystem(InputSystem& input);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    InputSystem& input_;
    bool openedByMenu_ = false;
};
