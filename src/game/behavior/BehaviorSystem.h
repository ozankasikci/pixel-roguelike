#pragma once

#include "engine/core/System.h"
#include "game/behavior/ActionTypes.h"

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct ActionEntry;
struct BehaviorComponent;

// Module action handler registration (per D-09)
// Modules register handlers for their action types at startup.
// Both BehaviorSystem (Application-based) and RuntimeGameplay (registry-based) use the same handlers.
using BehaviorActionHandler = std::function<void(entt::registry& registry,
                                                  entt::entity source,
                                                  entt::entity target,
                                                  ActionEntry& action)>;
void registerBehaviorActionHandler(ActionType type, BehaviorActionHandler handler);
BehaviorActionHandler* findBehaviorActionHandler(ActionType type);

struct PendingAction {
    float fireTime;
    entt::entity source;
    std::string eventType;  // "activate", "enter", "exit", "timer"
    std::size_t actionIndex;
};

// Events published on the EventBus by BehaviorSystem
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

class BehaviorSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    void processNewActivations(Application& app, float currentTime);
    void processTriggerFlags(Application& app, float currentTime);
    void processDelayedActions(Application& app, float currentTime);
    void executeActionList(Application& app, entt::entity source,
                           std::vector<ActionEntry>& actions,
                           const std::string& eventType, float currentTime);
    void executeAction(Application& app, entt::entity source, ActionEntry& action);
    void schedulePendingAction(entt::entity source, const std::string& eventType,
                               std::size_t actionIndex, float fireTime);

    std::vector<PendingAction> pendingActions_;
};
