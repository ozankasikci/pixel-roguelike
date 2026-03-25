#include "game/session/EquipmentState.h"

#include "game/content/ContentRegistry.h"
#include "game/session/RunSession.h"

namespace {

std::string& equippedHandRef(RunSession& session, EquipmentHand hand) {
    return hand == EquipmentHand::Right ? session.equippedRightHandWeaponId : session.equippedLeftHandWeaponId;
}

const std::string& equippedHandRef(const RunSession& session, EquipmentHand hand) {
    return hand == EquipmentHand::Right ? session.equippedRightHandWeaponId : session.equippedLeftHandWeaponId;
}

bool isTwoHandedWeaponId(const ContentRegistry& content, const std::string& weaponId) {
    if (weaponId.empty()) {
        return false;
    }

    const auto* weapon = content.findWeapon(weaponId);
    return weapon != nullptr && weapon->handedness == WeaponHandedness::TwoHanded;
}

void clearTwoHandedOccupancy(RunSession& session, const ContentRegistry& content) {
    if (session.equippedRightHandWeaponId.empty() || session.equippedRightHandWeaponId != session.equippedLeftHandWeaponId) {
        return;
    }
    if (!isTwoHandedWeaponId(content, session.equippedRightHandWeaponId)) {
        return;
    }

    session.equippedRightHandWeaponId.clear();
    session.equippedLeftHandWeaponId.clear();
}

} // namespace

bool ownsWeapon(const RunSession& session, const std::string& weaponId) {
    for (const auto& entry : session.ownedWeapons) {
        if (entry.definitionId == weaponId) {
            return true;
        }
    }
    return false;
}

bool equipWeapon(RunSession& session, const ContentRegistry& content, EquipmentHand hand, const std::string& weaponId) {
    if (!ownsWeapon(session, weaponId)) {
        return false;
    }

    const auto* weapon = content.findWeapon(weaponId);
    if (weapon == nullptr) {
        return false;
    }

    clearTwoHandedOccupancy(session, content);

    if (weapon->handedness == WeaponHandedness::TwoHanded) {
        session.equippedRightHandWeaponId = weaponId;
        session.equippedLeftHandWeaponId = weaponId;
        return true;
    }

    if (session.equippedRightHandWeaponId == weaponId) {
        session.equippedRightHandWeaponId.clear();
    }
    if (session.equippedLeftHandWeaponId == weaponId) {
        session.equippedLeftHandWeaponId.clear();
    }

    equippedHandRef(session, hand) = weaponId;
    return true;
}

bool unequipWeapon(RunSession& session, const ContentRegistry& content, EquipmentHand hand) {
    if (equippedHandRef(session, hand).empty()) {
        return false;
    }

    clearTwoHandedOccupancy(session, content);
    if (equippedHandRef(session, hand).empty()) {
        return true;
    }

    equippedHandRef(session, hand).clear();
    return true;
}

EffectiveEquipmentView resolveEffectiveEquipment(const RunSession& session, const ContentRegistry& content) {
    EffectiveEquipmentView view;
    view.rightHandWeaponId = session.equippedRightHandWeaponId;
    view.leftHandWeaponId = session.equippedLeftHandWeaponId;

    if (!view.rightHandWeaponId.empty() && view.rightHandWeaponId == view.leftHandWeaponId && isTwoHandedWeaponId(content, view.rightHandWeaponId)) {
        view.twoHanded = true;
        if (const auto* weapon = content.findWeapon(view.rightHandWeaponId)) {
            view.burden = weapon->equipWeight;
        }
        return view;
    }

    if (!view.rightHandWeaponId.empty()) {
        if (const auto* weapon = content.findWeapon(view.rightHandWeaponId)) {
            view.burden += weapon->equipWeight;
        }
    }
    if (!view.leftHandWeaponId.empty()) {
        if (const auto* weapon = content.findWeapon(view.leftHandWeaponId)) {
            view.burden += weapon->equipWeight;
        }
    }

    return view;
}

float computeEquipBurden(const RunSession& session, const ContentRegistry& content) {
    return resolveEffectiveEquipment(session, content).burden;
}
