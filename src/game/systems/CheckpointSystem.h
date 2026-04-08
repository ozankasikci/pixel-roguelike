#pragma once

#include "engine/core/System.h"

class CheckpointSystem : public System {
public:
    CheckpointSystem() = default;

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;
};
