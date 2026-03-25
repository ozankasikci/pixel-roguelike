#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <vector>
#include <entt/entt.hpp>
#include "engine/core/Window.h"
#include "engine/core/EventBus.h"
#include "engine/core/Time.h"

class SceneManager;
class System;

class Application {
public:
    enum class UpdatePhase : std::size_t {
        Input = 0,
        Interaction,
        Physics,
        Gameplay,
        Camera,
        Render,
        Count
    };

    Application(int width, int height, const char* title);
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();  // main game loop
    void requestQuit() { running_ = false; }
    void setSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

    // System management (execution is ordered by phase, then registration order within a phase)
    template<typename T, typename... Args>
    T& addSystem(UpdatePhase phase, Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        systemsByPhase_[static_cast<std::size_t>(phase)].push_back(std::move(system));
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
    std::array<std::vector<std::unique_ptr<System>>, static_cast<std::size_t>(UpdatePhase::Count)> systemsByPhase_;
    SceneManager* sceneManager_ = nullptr;
    bool running_ = false;
};
