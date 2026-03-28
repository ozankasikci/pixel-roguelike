#pragma once

#include "engine/core/System.h"

struct RuntimeInputState;

class CheckpointSystem : public System {
public:
    explicit CheckpointSystem(RuntimeInputState& input);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    RuntimeInputState& input_;
};
