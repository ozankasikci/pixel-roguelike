#pragma once
#include <entt/entt.hpp>

class Application;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void onEnter(Application& app) = 0;   // called when scene becomes active (spawn entities)
    virtual void onExit(Application& app) = 0;    // called when scene is removed (cleanup entities)
    virtual void onUpdate(Application& app, float deltaTime) {}  // optional per-frame scene logic
};
