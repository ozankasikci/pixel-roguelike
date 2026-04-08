#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/ActiveStateComponent.h"
#include "engine/ecs/HierarchyComponents.h"

namespace {

void disableRecursive(GameRegistry& registry, entt::entity entity) {
    if (!registry.all_of<DisabledTag>(entity)) {
        registry.emplace<DisabledTag>(entity);
    }
    auto* children = registry.try_get<ChildrenComponent>(entity);
    if (children) {
        for (auto child : children->children) {
            disableRecursive(registry, child);
        }
    }
}

void enableRecursive(GameRegistry& registry, entt::entity entity) {
    // If this entity was explicitly disabled, stop — it stays disabled
    auto* state = registry.try_get<ActiveStateComponent>(entity);
    if (state && !state->activeSelf) {
        return;
    }

    registry.raw().remove<DisabledTag>(entity);

    auto* children = registry.try_get<ChildrenComponent>(entity);
    if (children) {
        for (auto child : children->children) {
            enableRecursive(registry, child);
        }
    }
}

bool hasDisabledAncestor(const GameRegistry& registry, entt::entity entity) {
    auto* parent = registry.try_get<ParentComponent>(entity);
    if (!parent || parent->parent == entt::null) {
        return false;
    }
    if (registry.all_of<DisabledTag>(parent->parent)) {
        return true;
    }
    return hasDisabledAncestor(registry, parent->parent);
}

} // namespace

void setEntityActive(GameRegistry& registry, entt::entity entity, bool active) {
    auto& state = registry.get_or_emplace<ActiveStateComponent>(entity);
    state.activeSelf = active;

    if (!active) {
        disableRecursive(registry, entity);
    } else {
        // Only actually enable if no ancestor is disabled
        if (!hasDisabledAncestor(registry, entity)) {
            enableRecursive(registry, entity);
        }
    }
}

bool isActiveSelf(const GameRegistry& registry, entt::entity entity) {
    auto* state = registry.try_get<ActiveStateComponent>(entity);
    return state == nullptr || state->activeSelf;
}

bool isActiveInHierarchy(const GameRegistry& registry, entt::entity entity) {
    return !registry.all_of<DisabledTag>(entity);
}
