#include "game/systems/DoorSystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionFocusState.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

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

bool isPlayerLocked(entt::registry& registry, entt::entity player) {
    if (const auto* lock = registry.try_get<PlayerInteractionLockComponent>(player)) {
        return lock->active;
    }
    return false;
}

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* mesh = registry.try_get<MeshComponent>(entity);
    auto* collider = registry.try_get<StaticColliderComponent>(entity);
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    if (!mesh || !collider || !leaf) return;

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

DoorSystem::DoorSystem(InputSystem& input)
    : input_(input)
{}

void DoorSystem::init(Application& app) {
    auto& registry = app.registry();
    auto lockView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : lockView.each()) {
        lock.active = false;
        lock.remainingTime = 0.0f;
    }

    auto doorView = registry.view<DoorComponent>();
    for (auto [entity, door] : doorView.each()) {
        if (registry.try_get<DoorLeafComponent>(door.leftLeaf) != nullptr) {
            updateDoorLeaf(registry, door.leftLeaf, 0.0f);
        }
        if (registry.try_get<DoorLeafComponent>(door.rightLeaf) != nullptr) {
            updateDoorLeaf(registry, door.rightLeaf, 0.0f);
        }
    }
}

void DoorSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
    auto& focus = ctx.get<InteractionFocusState>();

    entt::entity playerEntity = entt::null;
    PlayerInteractionLockComponent* playerLock = nullptr;

    auto playerView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : playerView.each()) {
        playerEntity = entity;
        playerLock = &lock;
        break;
    }

    if (playerLock) {
        if (playerLock->active) {
            playerLock->remainingTime = std::max(0.0f, playerLock->remainingTime - deltaTime);
            if (playerLock->remainingTime <= 0.0f) {
                playerLock->active = false;
            }
        }
    }

    auto doorView = registry.view<TransformComponent, DoorComponent>();
    for (auto [entity, transform, door] : doorView.each()) {
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

    if (focus.focused == entt::null || !focus.activationRequested || focus.activationConsumed) {
        return;
    }

    auto* door = registry.try_get<DoorComponent>(focus.focused);
    if (door == nullptr) {
        return;
    }

    auto& interactable = registry.get<InteractableComponent>(focus.focused);
    auto& resolvedDoor = *door;
    if (resolvedDoor.opened || resolvedDoor.opening) {
        return;
    }

    if (isPlayerLocked(registry, playerEntity)) {
        return;
    }

    resolvedDoor.opening = true;
    resolvedDoor.progress = 0.0f;
    interactable.busy = true;
    focus.activationConsumed = true;

    if (playerLock) {
        playerLock->active = true;
        playerLock->remainingTime = resolvedDoor.openDuration * 0.9f;
    }
}

void DoorSystem::shutdown() {
}
