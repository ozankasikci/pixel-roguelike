#pragma once

#include "engine/ecs/GameRegistry.h"
#include "game/behavior/ActionTypes.h"

#include <cstddef>
#include <string>
#include <vector>

struct ActionEntry;
class GameplayEventSink;

struct GameplayPendingAction {
    float fireTime;
    entt::entity source;
    std::string eventType;
    std::size_t actionIndex;
};

class GameplayBehaviors {
public:
    void tick(GameRegistry& registry, float elapsedTime, GameplayEventSink& events);
    void reset();

private:
    void processNewActivations(GameRegistry& registry, float currentTime, GameplayEventSink& events);
    void processTriggerFlags(GameRegistry& registry, float currentTime, GameplayEventSink& events);
    void processDelayedActions(GameRegistry& registry, float currentTime, GameplayEventSink& events);
    void executeActionList(GameRegistry& registry, entt::entity source,
                           std::vector<ActionEntry>& actions,
                           const std::string& eventType, float currentTime,
                           GameplayEventSink& events);
    void executeAction(GameRegistry& registry, entt::entity source, ActionEntry& action,
                       GameplayEventSink& events);
    void schedulePendingAction(entt::entity source, const std::string& eventType,
                               std::size_t actionIndex, float fireTime);

    std::vector<GameplayPendingAction> pendingActions_;
};
