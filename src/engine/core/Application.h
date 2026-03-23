#pragma once
#include <memory>
#include <vector>
#include <entt/entt.hpp>
#include "engine/core/Window.h"
#include "engine/core/EventBus.h"
#include "engine/core/Time.h"

class System;

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();  // main game loop

    // System management (per D-05)
    template<typename T, typename... Args>
    T& addSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        systems_.push_back(std::move(system));
        return ref;
    }

    // Accessors (per D-06)
    Window& window() { return window_; }
    entt::registry& registry() { return registry_; }
    EventBus& eventBus() { return eventBus_; }
    const Time& time() const { return time_; }
    float deltaTime() const { return time_.deltaTime(); }

private:
    Window window_;
    entt::registry registry_;
    EventBus eventBus_;
    Time time_;
    std::vector<std::unique_ptr<System>> systems_;
    bool running_ = false;
};
