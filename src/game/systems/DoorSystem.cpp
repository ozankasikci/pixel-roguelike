#include "game/systems/DoorSystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionPromptState.h"

#include <GLFW/glfw3.h>
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

glm::vec3 cameraForwardFromAngles(float yawDeg, float pitchDeg) {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    forward.y = std::sin(glm::radians(pitchDeg));
    forward.z = std::sin(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    return glm::normalize(forward);
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
    auto& ctx = app.registry().ctx();
    if (ctx.contains<InteractionPromptState>()) {
        ctx.insert_or_assign(InteractionPromptState{});
    } else {
        ctx.emplace<InteractionPromptState>();
    }

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
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    auto& prompt = ctx.get<InteractionPromptState>();
    prompt.visible = false;
    prompt.busy = false;
    prompt.text.clear();

    entt::entity playerEntity = entt::null;
    TransformComponent* playerTransform = nullptr;
    CameraComponent* playerCamera = nullptr;
    PlayerInteractionLockComponent* playerLock = nullptr;

    auto playerView = registry.view<TransformComponent, CameraComponent, PlayerInteractionLockComponent>();
    for (auto [entity, transform, camera, lock] : playerView.each()) {
        playerEntity = entity;
        playerTransform = &transform;
        playerCamera = &camera;
        playerLock = &lock;
        break;
    }

    if (playerLock) {
        if (playerLock->active) {
            playerLock->remainingTime = std::max(0.0f, playerLock->remainingTime - deltaTime);
            if (playerLock->remainingTime <= 0.0f) {
                playerLock->active = false;
            } else {
                prompt.visible = true;
                prompt.busy = true;
                prompt.text = "OPENING HEAVY DOOR";
            }
        }
    }

    if (!playerTransform || !playerCamera) {
        return;
    }

    const glm::vec3 playerForward = cameraForwardFromAngles(playerCamera->yaw, playerCamera->pitch);

    entt::entity focusedDoor = entt::null;
    float bestScore = -1.0f;

    auto doorView = registry.view<TransformComponent, DoorComponent>();
    for (auto [entity, transform, door] : doorView.each()) {
        if (door.opening || door.opened) {
            door.progress = std::min(1.0f, door.progress + deltaTime / door.openDuration);
            updateDoorLeaf(registry, door.leftLeaf, door.progress);
            updateDoorLeaf(registry, door.rightLeaf, door.progress);

            if (door.progress >= 1.0f) {
                door.progress = 1.0f;
                door.opening = false;
                door.opened = true;
            }
            continue;
        }

        const glm::vec3 toDoor = transform.position - playerTransform->position;
        const float distance = glm::length(toDoor);
        if (distance > door.interactDistance || distance < 0.001f) {
            continue;
        }

        const glm::vec3 dirToDoor = toDoor / distance;
        const float facing = glm::dot(playerForward, dirToDoor);
        if (facing < door.interactDotThreshold) {
            continue;
        }

        const float score = facing - distance * 0.08f;
        if (score > bestScore) {
            bestScore = score;
            focusedDoor = entity;
        }
    }

    if (focusedDoor == entt::null) {
        return;
    }

    auto& door = registry.get<DoorComponent>(focusedDoor);
    if (door.opened || door.opening) {
        return;
    }

    prompt.visible = true;
    prompt.busy = false;
    prompt.text = "E  OPEN HEAVY DOOR";

    if (isPlayerLocked(registry, playerEntity)) {
        return;
    }

    if (input_.isCursorLocked() && !input_.wantsCaptureMouse() && input_.isKeyJustPressed(GLFW_KEY_E)) {
        door.opening = true;
        door.progress = 0.0f;

        if (playerLock) {
            playerLock->active = true;
            playerLock->remainingTime = door.openDuration * 0.9f;
        }

        prompt.visible = true;
        prompt.busy = true;
        prompt.text = "OPENING HEAVY DOOR";
    }
}

void DoorSystem::shutdown() {
}
