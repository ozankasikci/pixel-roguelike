#include "game/behavior/DoorAnimationSystem.h"

#include "engine/core/Application.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/ColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/prefabs/GameplayPrefabs.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* mesh = registry.try_get<MeshComponent>(entity);
    auto* collider = registry.try_get<ColliderComponent>(entity);
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    if (!mesh || !leaf) {
        return;
    }

    const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
    const float yaw = glm::mix(leaf->closedYaw, leaf->openYaw, eased);

    // Single source of truth: makePivotLeafModel with interpolated yaw
    const glm::mat4 model = makePivotLeafModel(leaf->basePosition, yaw, leaf->pivot, leaf->closedScale);

    mesh->modelOverride = model;
    mesh->useModelOverride = true;

    if (collider) {
        collider->position = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
        collider->halfExtents = leaf->colliderHalfExtents;
    }
}

} // namespace

void DoorAnimationSystem::init(Application& app) {
    auto& registry = app.registry();

    // Initialize player interaction locks to inactive
    auto lockView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : lockView.each()) {
        (void)entity;
        lock.active = false;
        lock.remainingTime = 0.0f;
    }

    // Initialize door leaf positions to closed state (progress=0)
    auto doorView = registry.view<DoorComponent>();
    for (auto [entity, door] : doorView.each()) {
        (void)entity;
        updateDoorLeaf(registry, door.leftLeaf, 0.0f);
        updateDoorLeaf(registry, door.rightLeaf, 0.0f);
    }
}

void DoorAnimationSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();

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

    // Animate all doors that are opening (BehaviorSystem sets opening=true)
    auto doorView = registry.view<TransformComponent, DoorComponent>();
    for (auto [entity, transform, door] : doorView.each()) {
        (void)transform;
        if (door.opening || door.opened) {
            door.progress = std::min(1.0f, door.progress + deltaTime / door.openDuration);
            updateDoorLeaf(registry, door.leftLeaf, door.progress);
            updateDoorLeaf(registry, door.rightLeaf, door.progress);

            if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                interactable->busy = door.opening;
                interactable->enabled = !door.opened;
            }

            if (door.progress >= 1.0f) {
                door.progress = 1.0f;
                door.opening = false;
                door.opened = true;
                if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                    interactable->busy = false;
                    interactable->enabled = false;
                }
            }
            continue;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->busy = false;
            interactable->enabled = !door.opened;
        }
    }
}

void DoorAnimationSystem::shutdown() {
}
