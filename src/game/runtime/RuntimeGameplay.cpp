#include "game/runtime/RuntimeGameplay.h"

#include "engine/input/InputSystem.h"
#include "game/components/PlayerTag.h"
#include "game/content/ContentRegistry.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>

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
