#include "game/runtime/RuntimeGameplay.h"

#include "engine/input/InputSystem.h"
#include "game/components/ColliderComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/InteractableComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/modules/player_control/PlayerControlCamera.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InteractionFocusState.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

bool inventoryTogglePressed(const InputSystem& input) {
    return input.isKeyJustPressed(GLFW_KEY_I)
        || input.isKeyJustPressedByName("i")
        || input.isKeyJustPressedByName("I")
        || input.isKeyJustPressedByName("\xc4\xb1")
        || input.isKeyJustPressedByName("\xc4\xb0")
        || input.wasCharacterTyped('i')
        || input.wasCharacterTyped('I')
        || input.wasCharacterTyped(0x0130)
        || input.wasCharacterTyped(0x0131);
}

InventoryMenuState& ensureMenuState(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InventoryMenuState>()) {
        ctx.emplace<InventoryMenuState>();
    }
    return ctx.get<InventoryMenuState>();
}

RuntimeInventoryCaptureState& ensureInventoryCaptureState(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeInventoryCaptureState>()) {
        ctx.emplace<RuntimeInventoryCaptureState>();
    }
    return ctx.get<RuntimeInventoryCaptureState>();
}

bool hasPlayerEntity(GameRegistry& registry) {
    auto view = registry.view<PlayerTag>();
    for (auto entity : view) {
        (void)entity;
        return true;
    }
    return false;
}

void clampSelection(RunSession& session, InventoryMenuState& menu) {
    if (session.ownedWeapons.empty()) {
        menu.selectedItem = 0;
        return;
    }
    if (menu.selectedItem < 0) {
        menu.selectedItem = 0;
    }
    const int lastIndex = static_cast<int>(session.ownedWeapons.size()) - 1;
    if (menu.selectedItem > lastIndex) {
        menu.selectedItem = lastIndex;
    }
}

} // namespace

void initializeRuntimeInteraction(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
}

void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input) {
    auto& ctx = registry.ctx();
    initializeRuntimeInteraction(registry);

    auto& prompt = ctx.get<InteractionPromptState>();
    auto& focus = ctx.get<InteractionFocusState>();
    prompt.visible = false;
    prompt.busy = false;
    prompt.text.clear();
    focus = InteractionFocusState{};

    const bool inventoryOpen = ctx.contains<InventoryMenuState>() && ctx.get<InventoryMenuState>().open;
    if (inventoryOpen) {
        return;
    }

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
    const glm::vec3 actorForward = buildPlayerCameraForward(actorCamera->yaw, actorCamera->pitch);

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

    if (input.isCursorLocked() && !input.wantsCaptureMouse() && input.isKeyJustPressed(GLFW_KEY_E)) {
        focus.activationRequested = true;
    }
}

void initializeRuntimeInventory(GameRegistry& registry) {
    (void)ensureMenuState(registry);
    (void)ensureInventoryCaptureState(registry);
}

void updateRuntimeInventory(GameRegistry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content) {
    auto& menu = ensureMenuState(registry);
    auto& captureState = ensureInventoryCaptureState(registry);

    clampSelection(session, menu);

    if (inventoryTogglePressed(input)) {
        menu.open = !menu.open;
        if (menu.open) {
            input.setCursorLocked(false);
            captureState.openedByMenu = true;
        } else if (captureState.openedByMenu) {
            input.setCursorLocked(true);
            captureState.openedByMenu = false;
        }
    } else if (menu.open && input.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        menu.open = false;
        if (captureState.openedByMenu) {
            input.setCursorLocked(true);
            captureState.openedByMenu = false;
        }
    }

    if (menu.selectedCategory == 0) {
        menu.targetedHand = EquipmentHand::Right;
    } else if (menu.selectedCategory == 1) {
        menu.targetedHand = EquipmentHand::Left;
    }

    if (!menu.open || !hasPlayerEntity(registry)) {
        return;
    }

    switch (menu.pendingAction) {
    case InventoryMenuState::PendingActionType::Equip:
        if (!menu.pendingWeaponId.empty()) {
            equipWeapon(session, content, menu.pendingHand, menu.pendingWeaponId);
        }
        break;
    case InventoryMenuState::PendingActionType::Unequip:
        unequipWeapon(session, content, menu.pendingHand);
        break;
    case InventoryMenuState::PendingActionType::None:
        break;
    }

    menu.pendingAction = InventoryMenuState::PendingActionType::None;
    menu.pendingWeaponId.clear();
}
