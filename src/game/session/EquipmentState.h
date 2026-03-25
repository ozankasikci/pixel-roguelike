#pragma once

#include <string>

struct RunSession;
class ContentRegistry;

enum class EquipmentHand {
    Right,
    Left,
};

struct EffectiveEquipmentView {
    std::string rightHandWeaponId;
    std::string leftHandWeaponId;
    bool twoHanded = false;
    float burden = 0.0f;
};

bool ownsWeapon(const RunSession& session, const std::string& weaponId);
bool equipWeapon(RunSession& session, const ContentRegistry& content, EquipmentHand hand, const std::string& weaponId);
bool unequipWeapon(RunSession& session, const ContentRegistry& content, EquipmentHand hand);
EffectiveEquipmentView resolveEffectiveEquipment(const RunSession& session, const ContentRegistry& content);
float computeEquipBurden(const RunSession& session, const ContentRegistry& content);
