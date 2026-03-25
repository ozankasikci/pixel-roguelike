#include "game/systems/InventorySystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/PlayerTag.h"
#include "game/content/ContentRegistry.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace {

bool inventoryTogglePressed(const InputSystem& input) {
    return input.isKeyJustPressed(GLFW_KEY_I)
        || input.isKeyJustPressedByName("i")
        || input.isKeyJustPressedByName("I")
        || input.isKeyJustPressedByName("ı")
        || input.isKeyJustPressedByName("İ")
        || input.wasCharacterTyped('i')
        || input.wasCharacterTyped('I')
        || input.wasCharacterTyped(0x0130) // Turkish uppercase dotted I
        || input.wasCharacterTyped(0x0131); // Turkish lowercase dotless i
}

InventoryMenuState& ensureMenuState(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InventoryMenuState>()) {
        ctx.emplace<InventoryMenuState>();
    }
    return ctx.get<InventoryMenuState>();
}

bool hasPlayerEntity(entt::registry& registry) {
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

InventorySystem::InventorySystem(InputSystem& input)
    : input_(input) {}

void InventorySystem::init(Application& app) {
    (void)ensureMenuState(app.registry());
}

void InventorySystem::update(Application& app, float deltaTime) {
    (void)deltaTime;

    auto& registry = app.registry();
    auto& menu = ensureMenuState(registry);
    auto& session = app.getService<RunSession>();
    auto& content = app.getService<ContentRegistry>();

    clampSelection(session, menu);

    if (inventoryTogglePressed(input_)) {
        menu.open = !menu.open;
        if (menu.open) {
            input_.unlockCursor();
            openedByMenu_ = true;
        } else if (openedByMenu_) {
            input_.lockCursor();
            openedByMenu_ = false;
        }
    } else if (menu.open && input_.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        menu.open = false;
        if (openedByMenu_) {
            input_.lockCursor();
            openedByMenu_ = false;
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

void InventorySystem::shutdown() {}
