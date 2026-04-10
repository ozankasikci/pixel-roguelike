#include "game/modules/interaction/InteractionSystem.h"

#include "engine/camera/CameraManager.h"
#include "engine/input/InputSystem.h"
#include "game/components/ControllableTag.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/modules/interaction/InteractionFocusState.h"
#include "game/modules/interaction/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>

namespace {

InteractionPromptState& ensurePromptState(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    return ctx.get<InteractionPromptState>();
}

InteractionFocusState& ensureFocusState(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
    return ctx.get<InteractionFocusState>();
}

struct InteractionActorContext {
    entt::entity entity = entt::null;
    TransformComponent* transform = nullptr;
    PlayerInteractionLockComponent* lock = nullptr;
};

InteractionActorContext resolveInteractionActor(GameRegistry& registry) {
    auto view = registry.view<TransformComponent,
                              ControllableTag,
                              PlayerTag>();
    for (auto entity : view) {
        return InteractionActorContext{
            .entity = entity,
            .transform = &view.get<TransformComponent>(entity),
            .lock = registry.try_get<PlayerInteractionLockComponent>(entity),
        };
    }
    return {};
}

} // namespace

void initializeRuntimeInteraction(GameRegistry& registry) {
    (void)ensurePromptState(registry);
    (void)ensureFocusState(registry);
}

void resetRuntimeInteraction(GameRegistry& registry) {
    ensurePromptState(registry) = InteractionPromptState{};
    ensureFocusState(registry) = InteractionFocusState{};
}

void clearRuntimeInteraction(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (ctx.contains<InteractionPromptState>()) {
        ctx.erase<InteractionPromptState>();
    }
    if (ctx.contains<InteractionFocusState>()) {
        ctx.erase<InteractionFocusState>();
    }
}

InteractionPromptState& ensureInteractionPromptState(GameRegistry& registry) {
    return ensurePromptState(registry);
}

void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input,
                              const CameraManager& cameraManager) {
    auto& ctx = registry.ctx();
    auto& prompt = ensurePromptState(registry);
    auto& focus = ensureFocusState(registry);
    prompt = InteractionPromptState{};
    focus = InteractionFocusState{};

    const bool inventoryOpen = ctx.contains<InventoryMenuState>()
        && ctx.get<InventoryMenuState>().open;
    if (inventoryOpen) {
        return;
    }

    const InteractionActorContext actor = resolveInteractionActor(registry);
    if (actor.entity == entt::null || actor.transform == nullptr) {
        return;
    }

    focus.actor = actor.entity;
    const glm::vec3 actorForward = cameraManager.getState().forward;

    float bestScore = -1.0f;
    auto interactableView = registry.view<TransformComponent, InteractableComponent>();
    for (auto [entity, transform, interactable] : interactableView.each()) {
        if (!interactable.enabled) {
            continue;
        }

        const glm::vec3 toTarget = transform.position - actor.transform->position;
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

    const auto& interactable = registry.get<InteractableComponent>(focus.focused);
    prompt.visible = true;
    prompt.busy = interactable.busy;
    prompt.text = interactable.busy ? interactable.busyText : interactable.promptText;

    const bool locked = actor.lock != nullptr && actor.lock->active;
    if (locked || interactable.busy) {
        return;
    }

    if (input.isCursorLocked()
        && !input.wantsCaptureMouse()
        && input.isKeyJustPressed(GLFW_KEY_E)) {
        focus.activationRequested = true;
    }
}
