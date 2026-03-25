#include "game/systems/CheckpointSystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionPromptState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace {

glm::vec3 cameraForwardFromAngles(float yawDeg, float pitchDeg) {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    forward.y = std::sin(glm::radians(pitchDeg));
    forward.z = std::sin(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    return glm::normalize(forward);
}

} // namespace

CheckpointSystem::CheckpointSystem(InputSystem& input)
    : input_(input)
{}

void CheckpointSystem::init(Application& app) {
    auto& ctx = app.registry().ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
}

void CheckpointSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    auto& prompt = ctx.get<InteractionPromptState>();

    if (messageTimer_ > 0.0f) {
        messageTimer_ = std::max(0.0f, messageTimer_ - deltaTime);
        prompt.visible = true;
        prompt.busy = true;
        prompt.text = "CHECKPOINT KINDLED";
    }

    entt::entity playerEntity = entt::null;
    TransformComponent* playerTransform = nullptr;
    CameraComponent* playerCamera = nullptr;
    PlayerSpawnComponent* playerSpawn = nullptr;

    auto playerView = registry.view<TransformComponent, CameraComponent, PlayerSpawnComponent>();
    for (auto [entity, transform, camera, spawn] : playerView.each()) {
        playerEntity = entity;
        playerTransform = &transform;
        playerCamera = &camera;
        playerSpawn = &spawn;
        break;
    }

    if (!playerTransform || !playerCamera || !playerSpawn) {
        return;
    }

    entt::entity focusedCheckpoint = entt::null;
    float bestScore = -1.0f;
    const glm::vec3 playerForward = cameraForwardFromAngles(playerCamera->yaw, playerCamera->pitch);

    auto checkpointView = registry.view<TransformComponent, CheckpointComponent>();
    for (auto [entity, transform, checkpoint] : checkpointView.each()) {
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity)) {
            light->intensity = checkpoint.active ? 2.2f : 1.15f;
            light->radius = checkpoint.active ? 10.0f : 7.0f;
        }

        const glm::vec3 toCheckpoint = transform.position - playerTransform->position;
        const float distance = glm::length(toCheckpoint);
        if (distance > checkpoint.interactDistance || distance < 0.001f) {
            continue;
        }

        const glm::vec3 dir = toCheckpoint / distance;
        const float facing = glm::dot(playerForward, dir);
        if (facing < checkpoint.interactDotThreshold) {
            continue;
        }

        const float score = facing - distance * 0.1f;
        if (score > bestScore) {
            bestScore = score;
            focusedCheckpoint = entity;
        }
    }

    if (focusedCheckpoint == entt::null) {
        return;
    }

    auto& checkpoint = registry.get<CheckpointComponent>(focusedCheckpoint);
    if (!checkpoint.active || messageTimer_ > 0.0f) {
        prompt.visible = true;
        prompt.busy = false;
        prompt.text = checkpoint.active ? "RESPAWN ATTUNED" : "E  KINDLE CHECKPOINT";
    }

    if (input_.isCursorLocked() && !input_.wantsCaptureMouse() && input_.isKeyJustPressed(GLFW_KEY_E)) {
        auto checkpointViewAll = registry.view<CheckpointComponent>();
        for (auto [entity, other] : checkpointViewAll.each()) {
            other.active = false;
        }

        checkpoint.active = true;
        playerSpawn->respawnPosition = checkpoint.respawnPosition;
        messageTimer_ = 2.0f;
        prompt.visible = true;
        prompt.busy = true;
        prompt.text = "CHECKPOINT KINDLED";
    }
}

void CheckpointSystem::shutdown() {
}
