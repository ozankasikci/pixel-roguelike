#pragma once

#include "engine/core/System.h"

class DoorAnimationSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;
};
