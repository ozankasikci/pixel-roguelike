#pragma once

#include <entt/entt.hpp>
#include "engine/ecs/DisabledTag.h"

class GameRegistry {
public:
    // --- Filtered views (default for all game systems) ---

    template<typename... Components>
    auto view() {
        return registry_.view<Components...>(entt::exclude<DisabledTag>);
    }

    template<typename... Components>
    auto view() const {
        return registry_.view<Components...>(entt::exclude<DisabledTag>);
    }

    // --- Unfiltered views (editor, serialization, level loading) ---

    template<typename... Components>
    auto viewAll() {
        return registry_.view<Components...>();
    }

    template<typename... Components>
    auto viewAll() const {
        return registry_.view<Components...>();
    }

    // --- Entity lifecycle ---

    entt::entity create() { return registry_.create(); }
    void destroy(entt::entity e) { registry_.destroy(e); }
    bool valid(entt::entity e) const { return registry_.valid(e); }

    // --- Component access ---

    template<typename T, typename... Args>
    decltype(auto) emplace(entt::entity e, Args&&... args) {
        return registry_.emplace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& emplace_or_replace(entt::entity e, Args&&... args) {
        return registry_.emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& get_or_emplace(entt::entity e, Args&&... args) {
        return registry_.get_or_emplace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T& get(entt::entity e) { return registry_.get<T>(e); }

    template<typename T>
    const T& get(entt::entity e) const { return registry_.get<T>(e); }

    template<typename T>
    T* try_get(entt::entity e) { return registry_.try_get<T>(e); }

    template<typename T>
    const T* try_get(entt::entity e) const { return registry_.try_get<T>(e); }

    template<typename... T>
    bool all_of(entt::entity e) const { return registry_.all_of<T...>(e); }

    template<typename... T>
    bool any_of(entt::entity e) const { return registry_.any_of<T...>(e); }

    template<typename... T>
    std::size_t remove(entt::entity e) { return registry_.remove<T...>(e); }

    template<typename T, typename... Func>
    T& patch(entt::entity e, Func&&... func) {
        return registry_.patch<T>(e, std::forward<Func>(func)...);
    }

    // --- Context variables ---

    auto& ctx() { return registry_.ctx(); }
    const auto& ctx() const { return registry_.ctx(); }

    // --- Escape hatch for unwrapped EnTT API ---

    entt::registry& raw() { return registry_; }
    const entt::registry& raw() const { return registry_; }

private:
    entt::registry registry_;
};
