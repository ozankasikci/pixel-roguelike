#include "game/modules/door/DoorAnimationSystem.h"

#include "engine/core/Application.h"
#include "game/modules/door/DoorComponents.h"
#include "game/modules/door/DoorMath.h"
#include "game/components/InteractableComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/ColliderComponent.h"
#include "game/components/TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

float getDoorLeafYaw(const DoorLeafComponent& leaf, float progress) {
    const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
    return glm::mix(leaf.closedYaw, leaf.openYaw, eased);
}

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* mesh = registry.try_get<MeshComponent>(entity);
    auto* collider = registry.try_get<ColliderComponent>(entity);
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    if (!mesh || !collider || !leaf) {
        return;
    }

    const float yaw = getDoorLeafYaw(*leaf, progress);
    const glm::mat4 model = makePivotLeafModel(leaf->basePosition, leaf->closedYaw, yaw,
                                               leaf->pivot, leaf->meshCenter, leaf->closedScale);
    const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    mesh->modelOverride = model;
    mesh->useModelOverride = true;

    collider->position = center;
    collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
    collider->halfExtents = leaf->colliderHalfExtents;
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

    // Initialize door leaf positions to closed state
    auto doorView = registry.view<DoorConfigComponent, DoorStateComponent>();
    for (auto [entity, config, state] : doorView.each()) {
        (void)entity;
        state.progress = 0.0f;
        state.targetState = DoorTargetState::Closed;
        updateDoorLeaf(registry, config.leftLeaf, 0.0f);
        updateDoorLeaf(registry, config.rightLeaf, 0.0f);
    }
}

void DoorAnimationSystem::update(Application& app, float deltaTime) {
    tickDoorAnimation(app.registry(), deltaTime);
}

void DoorAnimationSystem::shutdown() {
}
