#include "game/content/ContentRegistry.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "common/TestSupport.h"

#include <cassert>

int main() {
    ContentRegistry content;
    content.loadDefaults();

    RunSession session;
    assert(session.ownedWeapons.size() == 2);
    assert(ownsWeapon(session, "old_dagger"));
    assert(ownsWeapon(session, "brigand_axe"));
    assert(!ownsWeapon(session, "missing_weapon"));
    assert(test_support::nearlyEqual(computeEquipBurden(session, content), 0.0f));

    assert(equipWeapon(session, content, EquipmentHand::Right, "old_dagger"));
    auto view = resolveEffectiveEquipment(session, content);
    assert(view.rightHandWeaponId == "old_dagger");
    assert(view.leftHandWeaponId.empty());
    assert(!view.twoHanded);
    assert(test_support::nearlyEqual(view.burden, 2.0f));

    assert(equipWeapon(session, content, EquipmentHand::Left, "brigand_axe"));
    view = resolveEffectiveEquipment(session, content);
    assert(view.rightHandWeaponId == "brigand_axe");
    assert(view.leftHandWeaponId == "brigand_axe");
    assert(view.twoHanded);
    assert(test_support::nearlyEqual(view.burden, 6.5f));

    assert(equipWeapon(session, content, EquipmentHand::Right, "old_dagger"));
    view = resolveEffectiveEquipment(session, content);
    assert(view.rightHandWeaponId == "old_dagger");
    assert(view.leftHandWeaponId.empty());
    assert(!view.twoHanded);
    assert(test_support::nearlyEqual(view.burden, 2.0f));

    assert(equipWeapon(session, content, EquipmentHand::Left, "old_dagger"));
    view = resolveEffectiveEquipment(session, content);
    assert(view.rightHandWeaponId.empty());
    assert(view.leftHandWeaponId == "old_dagger");
    assert(!view.twoHanded);
    assert(test_support::nearlyEqual(view.burden, 2.0f));

    assert(unequipWeapon(session, content, EquipmentHand::Left));
    view = resolveEffectiveEquipment(session, content);
    assert(view.rightHandWeaponId.empty());
    assert(view.leftHandWeaponId.empty());
    assert(test_support::nearlyEqual(view.burden, 0.0f));

    return 0;
}
