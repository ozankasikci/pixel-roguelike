#pragma once

#include "engine/core/System.h"

struct RuntimeInputState;

class InventorySystem : public System {
public:
    explicit InventorySystem(RuntimeInputState& input);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    RuntimeInputState& input_;
};
