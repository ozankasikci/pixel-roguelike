#include "game/systems/InteractionSystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionFocusState.h"
#include "game/ui/InteractionPromptState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
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

InteractionSystem::InteractionSystem(InputSystem& input)
    : input_(input)
{}

void InteractionSystem::init(Application& app) {
    auto& ctx = app.registry().ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
}

void InteractionSystem::update(Application& app, float deltaTime) {
    (void)deltaTime;

    auto& registry = app.registry();
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }

    auto& prompt = ctx.get<InteractionPromptState>();
    auto& focus = ctx.get<InteractionFocusState>();
    prompt.visible = false;
    prompt.busy = false;
    prompt.text.clear();
    focus = InteractionFocusState{};

    entt::entity actor = entt::null;
    TransformComponent* actorTransform = nullptr;
    CameraComponent* actorCamera = nullptr;
    PlayerInteractionLockComponent* actorLock = nullptr;

    auto actorView = registry.view<TransformComponent, CameraComponent, ControllableTag, PrimaryCameraTag>();
    for (auto entity : actorView) {
        actor = entity;
        actorTransform = &actorView.get<TransformComponent>(entity);
        actorCamera = &actorView.get<CameraComponent>(entity);
        actorLock = registry.try_get<PlayerInteractionLockComponent>(entity);
        break;
    }

    if (actor == entt::null || actorTransform == nullptr || actorCamera == nullptr) {
        return;
    }

    focus.actor = actor;
    const glm::vec3 actorForward = cameraForwardFromAngles(actorCamera->yaw, actorCamera->pitch);

    float bestScore = -1.0f;
    auto interactableView = registry.view<TransformComponent, InteractableComponent>();
    for (auto [entity, transform, interactable] : interactableView.each()) {
        if (!interactable.enabled) {
            continue;
        }

        const glm::vec3 toTarget = transform.position - actorTransform->position;
        const float distance = glm::length(toTarget);
        if (distance > interactable.interactDistance || distance < 0.001f) {
            continue;
        }

        const glm::vec3 direction = toTarget / distance;
        const float facing = glm::dot(actorForward, direction);
        if (facing < interactable.interactDotThreshold) {
            continue;
        }

        const float score = facing - distance * 0.08f;
        if (score > bestScore) {
            bestScore = score;
            focus.focused = entity;
        }
    }

    if (focus.focused == entt::null) {
        return;
    }

    auto& interactable = registry.get<InteractableComponent>(focus.focused);
    prompt.visible = true;
    prompt.busy = interactable.busy;
    prompt.text = interactable.busy ? interactable.busyText : interactable.promptText;

    const bool locked = actorLock != nullptr && actorLock->active;
    if (locked || interactable.busy) {
        return;
    }

    if (input_.isCursorLocked() && !input_.wantsCaptureMouse() && input_.isKeyJustPressed(GLFW_KEY_E)) {
        focus.activationRequested = true;
    }
}

void InteractionSystem::shutdown() {
}
