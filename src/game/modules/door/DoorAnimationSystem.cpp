#include "game/modules/door/DoorAnimationSystem.h"

#include "engine/core/Application.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PivotTransformComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/TransformComponent.h"
#include "game/modules/door/DoorComponents.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace {

float getDoorLeafYaw(const DoorLeafComponent& leaf, const PivotTransformComponent& pivot,
                     float progress) {
    const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
    return glm::mix(pivot.closedYawDeg, leaf.openYaw, eased);
}

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    auto* pivot = registry.try_get<PivotTransformComponent>(entity);
    if (!leaf || !pivot) {
        return;
    }
    pivot->currentYawDeg = getDoorLeafYaw(*leaf, *pivot, progress);
}

} // namespace

void tickDoorAnimation(entt::registry& registry, float deltaTime) {
    // Tick PlayerInteractionLock countdown timer
    auto lockView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : lockView.each()) {
        if (lock.active) {
            lock.remainingTime = std::max(0.0f, lock.remainingTime - deltaTime);
            if (lock.remainingTime <= 0.0f) {
                lock.active = false;
            }
        }
    }

    // Animate all doors based on targetState (bidirectional)
    auto doorView = registry.view<TransformComponent, DoorConfigComponent, DoorStateComponent>();
    for (auto [entity, transform, config, state] : doorView.each()) {
        (void)transform;

        if (isDoorMoving(state)) {
            // Advance or retreat progress based on targetState
            const float step = deltaTime / config.openDuration;
            if (state.targetState == DoorTargetState::Open) {
                state.progress = std::min(1.0f, state.progress + step);
            } else {
                state.progress = std::max(0.0f, state.progress - step);
            }

            updateDoorLeaf(registry, config.leftLeaf, state.progress);
            updateDoorLeaf(registry, config.rightLeaf, state.progress);

            if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                interactable->busy = isDoorMoving(state);
                interactable->enabled = false; // not interactable while moving
            }

            // Check if reached target
            if (isDoorFullyOpen(state) || isDoorFullyClosed(state)) {
                if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                    interactable->busy = false;
                    interactable->enabled = !isDoorFullyOpen(state); // re-enable when fully closed
                }
            }
            continue;
        }

        // Not moving -- set interactable state
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->busy = false;
            interactable->enabled = !isDoorFullyOpen(state);
        }
    }
}

void DoorAnimationSystem::init(Application& app) {
    auto& registry = app.registry();

    auto lockView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : lockView.each()) {
        (void)entity;
        lock.active = false;
        lock.remainingTime = 0.0f;
    }

    // Initialize door leaf pivots to closed state
    auto doorView = registry.view<DoorConfigComponent, DoorStateComponent>();
    for (auto [entity, config, state] : doorView.each()) {
        (void)entity;
        state.progress = 0.0f;
        state.targetState = DoorTargetState::Closed;
        if (auto* pivot = registry.try_get<PivotTransformComponent>(config.leftLeaf)) {
            pivot->currentYawDeg = pivot->closedYawDeg;
        }
        if (auto* pivot = registry.try_get<PivotTransformComponent>(config.rightLeaf)) {
            pivot->currentYawDeg = pivot->closedYawDeg;
        }
    }
}

void DoorAnimationSystem::update(Application& app, float deltaTime) {
    tickDoorAnimation(app.registry(), deltaTime);
}

void DoorAnimationSystem::shutdown() {
}

void resetDoorVisuals(entt::registry& registry) {
    auto doorView = registry.view<DoorConfigComponent, DoorStateComponent>();
    for (auto [entity, config, state] : doorView.each()) {
        (void)entity;
        updateDoorLeaf(registry, config.leftLeaf, state.progress);
        updateDoorLeaf(registry, config.rightLeaf, state.progress);
    }
}
