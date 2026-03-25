#pragma once

#include "game/session/EquipmentState.h"

#include <string>

struct InventoryMenuState {
    enum class PendingActionType {
        None,
        Equip,
        Unequip,
    };

    bool open = false;
    int selectedCategory = 0;
    int selectedItem = 0;
    EquipmentHand targetedHand = EquipmentHand::Right;
    PendingActionType pendingAction = PendingActionType::None;
    EquipmentHand pendingHand = EquipmentHand::Right;
    std::string pendingWeaponId;
};
