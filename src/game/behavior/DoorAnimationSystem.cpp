#include "game/behavior/DoorAnimationSystem.h"

#include "engine/core/Application.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
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

glm::mat4 makeDoorLeafModel(const DoorLeafComponent& leaf, float progress) {
    const float yaw = getDoorLeafYaw(leaf, progress);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), leaf.hingePosition);
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, leaf.centerOffsetFromHinge);
    model = glm::scale(model, leaf.closedScale);
    return model;
}

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* mesh = registry.try_get<MeshComponent>(entity);
    auto* collider = registry.try_get<ColliderComponent>(entity);
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    if (!mesh || !collider || !leaf) {
        return;
    }

    const float yaw = getDoorLeafYaw(*leaf, progress);
    const glm::mat4 model = makeDoorLeafModel(*leaf, progress);
    const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    mesh->modelOverride = model;
    mesh->useModelOverride = true;

    collider->position = center;
    collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
    collider->halfExtents = leaf->colliderHalfExtents;
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
        if (registry.try_get<DoorLeafComponent>(door.leftLeaf) != nullptr) {
            // updateDoorLeaf at progress 0 initializes the leaf mesh/collider position
            auto* mesh = registry.try_get<MeshComponent>(door.leftLeaf);
            auto* collider = registry.try_get<ColliderComponent>(door.leftLeaf);
            auto* leaf = registry.try_get<DoorLeafComponent>(door.leftLeaf);
            if (mesh && collider && leaf) {
                const float yaw = leaf->closedYaw;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), leaf->hingePosition);
                model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, leaf->centerOffsetFromHinge);
                model = glm::scale(model, leaf->closedScale);
                const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                mesh->modelOverride = model;
                mesh->useModelOverride = true;
                collider->position = center;
                collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
                collider->halfExtents = leaf->colliderHalfExtents;
            }
        }
        if (registry.try_get<DoorLeafComponent>(door.rightLeaf) != nullptr) {
            auto* mesh = registry.try_get<MeshComponent>(door.rightLeaf);
            auto* collider = registry.try_get<ColliderComponent>(door.rightLeaf);
            auto* leaf = registry.try_get<DoorLeafComponent>(door.rightLeaf);
            if (mesh && collider && leaf) {
                const float yaw = leaf->closedYaw;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), leaf->hingePosition);
                model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, leaf->centerOffsetFromHinge);
                model = glm::scale(model, leaf->closedScale);
                const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                mesh->modelOverride = model;
                mesh->useModelOverride = true;
                collider->position = center;
                collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
                collider->halfExtents = leaf->colliderHalfExtents;
            }
        }
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
    // This system does NOT check InteractionFocusState or activationRequested (per D-11)
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
