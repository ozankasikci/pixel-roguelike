#pragma once

#include "engine/core/System.h"

struct RuntimeInputState;

class DoorSystem : public System {
public:
    explicit DoorSystem(RuntimeInputState& input);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    RuntimeInputState& input_;
};
