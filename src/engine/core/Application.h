#pragma once
#include <any>
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
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

    template<typename T, typename... Args>
    T& emplaceService(Args&&... args) {
        auto key = std::type_index(typeid(T));
        services_[key] = T(std::forward<Args>(args)...);
        return std::any_cast<T&>(services_.at(key));
    }

    template<typename T>
    bool hasService() const {
        return services_.contains(std::type_index(typeid(T)));
    }

    template<typename T>
    T* tryGetService() {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    template<typename T>
    const T* tryGetService() const {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    template<typename T>
    T& getService() {
        T* service = tryGetService<T>();
        if (service == nullptr) {
            throw std::runtime_error("Application service not found");
        }
        return *service;
    }

    template<typename T>
    const T& getService() const {
        const T* service = tryGetService<T>();
        if (service == nullptr) {
            throw std::runtime_error("Application service not found");
        }
        return *service;
    }

private:
    Window window_;
    entt::registry registry_;
    EventBus eventBus_;
    Time time_;
    std::unordered_map<std::type_index, std::any> services_;
    std::array<std::vector<std::unique_ptr<System>>, static_cast<std::size_t>(UpdatePhase::Count)> systemsByPhase_;
    SceneManager* sceneManager_ = nullptr;
    bool running_ = false;
};
