#pragma once

#include <entt/entt.hpp>

class Application; // forward declare
class EventBus;    // forward declare
class Time;        // forward declare

class System {
public:
    virtual ~System() = default;
    virtual void init(Application& app) = 0;
    virtual void update(Application& app, float deltaTime) = 0;
    virtual void shutdown() = 0;
};
