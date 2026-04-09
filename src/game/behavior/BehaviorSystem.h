#pragma once

#include "engine/ecs/GameRegistry.h"
#include "game/behavior/ActionTypes.h"

#include <functional>
#include <string>

// Forward declarations
struct ActionEntry;

// Module action handler registration (per D-09)
// Modules register handlers for their action types at startup.
// GameplayBehaviors (owned by RuntimeGameSession) uses these handlers at runtime.
using BehaviorActionHandler = std::function<void(GameRegistry& registry,
                                                  entt::entity source,
                                                  entt::entity target,
                                                  ActionEntry& action)>;
void registerBehaviorActionHandler(ActionType type, BehaviorActionHandler handler);
BehaviorActionHandler* findBehaviorActionHandler(ActionType type);

// Events published on the EventBus by GameplayBehaviors
struct ShowMessageEvent {
    std::string text;
    float duration;
};

struct PlaySoundEvent {
    std::string soundId;
    float volume = 1.0f;
};

struct BehaviorEvent {
    std::string name;
};
