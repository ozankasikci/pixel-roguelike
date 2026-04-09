#include "game/runtime/GameplayBehaviors.h"

#include "game/runtime/GameplayEventSink.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/behavior/BehaviorSystem.h"  // for findBehaviorActionHandler
#include "game/behavior/NodeIndex.h"
#include "game/components/ColliderComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionFocusState.h"

#include <algorithm>
#include <cmath>

void GameplayBehaviors::tick(GameRegistry& registry, float elapsedTime,
                             GameplayEventSink& events) {
    processDelayedActions(registry, elapsedTime, events);
    processNewActivations(registry, elapsedTime, events);
    processTriggerFlags(registry, elapsedTime, events);
}

void GameplayBehaviors::reset() {
    pendingActions_.clear();
}

void GameplayBehaviors::processNewActivations(GameRegistry& registry, float currentTime,
                                              GameplayEventSink& events) {
    auto& ctx = registry.ctx();

    if (!ctx.contains<InteractionFocusState>()) {
        return;
    }
    auto& focus = ctx.get<InteractionFocusState>();

    if (!focus.activationRequested || focus.activationConsumed || focus.focused == entt::null) {
        return;
    }

    auto* behavior = registry.try_get<BehaviorComponent>(focus.focused);
    if (behavior == nullptr || !behavior->enabled) {
        return;
    }

    executeActionList(registry, focus.focused, behavior->onActivate, "activate", currentTime,
                      events);
    focus.activationConsumed = true;
}

void GameplayBehaviors::processTriggerFlags(GameRegistry& registry, float currentTime,
                                            GameplayEventSink& events) {
    auto view = registry.view<ColliderComponent, BehaviorComponent>();
    for (auto [entity, collider, behavior] : view.each()) {
        if (collider.mode == ColliderMode::Solid) continue;
        if (collider.pendingEnter) {
            executeActionList(registry, entity, behavior.onEnter, "enter", currentTime, events);
            collider.pendingEnter = false;
        }
        if (collider.pendingExit) {
            executeActionList(registry, entity, behavior.onExit, "exit", currentTime, events);
            collider.pendingExit = false;
        }
    }
}

void GameplayBehaviors::processDelayedActions(GameRegistry& registry, float currentTime,
                                              GameplayEventSink& events) {
    while (!pendingActions_.empty() && pendingActions_.front().fireTime <= currentTime) {
        GameplayPendingAction pending = pendingActions_.front();
        pendingActions_.erase(pendingActions_.begin());

        if (!registry.valid(pending.source)) {
            continue;
        }

        auto* behavior = registry.try_get<BehaviorComponent>(pending.source);
        if (behavior == nullptr) {
            continue;
        }

        std::vector<ActionEntry>* actionList = nullptr;
        if (pending.eventType == "activate") {
            actionList = &behavior->onActivate;
        } else if (pending.eventType == "enter") {
            actionList = &behavior->onEnter;
        } else if (pending.eventType == "exit") {
            actionList = &behavior->onExit;
        } else if (pending.eventType == "timer") {
            actionList = &behavior->onTimer;
        }

        if (actionList == nullptr || pending.actionIndex >= actionList->size()) {
            continue;
        }

        auto& action = (*actionList)[pending.actionIndex];
        if (action.fireOnce && action.fired) {
            continue;
        }
        executeAction(registry, pending.source, action, events);
        if (action.fireOnce) {
            action.fired = true;
        }
    }
}

void GameplayBehaviors::executeActionList(GameRegistry& registry, entt::entity source,
                                          std::vector<ActionEntry>& actions,
                                          const std::string& eventType, float currentTime,
                                          GameplayEventSink& events) {
    float accumulatedDelay = 0.0f;

    for (std::size_t i = 0; i < actions.size(); ++i) {
        auto& entry = actions[i];

        if (entry.fireOnce && entry.fired) {
            continue;
        }

        if (entry.type == ActionType::Delay) {
            // Delay action accumulates delay for subsequent actions
            const auto* delayParams = std::get_if<DelayActionParams>(&entry.params);
            if (delayParams) {
                accumulatedDelay += delayParams->seconds;
            }
            if (entry.fireOnce) {
                entry.fired = true;
            }
            continue;
        }

        const float totalDelay = entry.delay + accumulatedDelay;

        if (totalDelay > 0.0f) {
            schedulePendingAction(source, eventType, i, currentTime + totalDelay);
            // fireOnce marking for delayed actions is deferred to when they actually fire
        } else {
            executeAction(registry, source, entry, events);
            if (entry.fireOnce) {
                entry.fired = true;
            }
        }
    }
}

void GameplayBehaviors::executeAction(GameRegistry& registry, entt::entity source,
                                      ActionEntry& action, GameplayEventSink& events) {
    auto& ctx = registry.ctx();

    // Resolve target entity
    entt::entity target = source;
    if (!action.targetNodeId.empty() && action.targetNodeId != "self") {
        if (ctx.contains<NodeIndex>()) {
            const auto& nodeIndex = ctx.get<NodeIndex>();
            target = nodeIndex.resolve(action.targetNodeId, source);
        }
    }

    switch (action.type) {
    case ActionType::OpenDoor:
    case ActionType::CloseDoor:
    case ActionType::ToggleDoor: {
        auto* handler = findBehaviorActionHandler(action.type);
        if (handler) {
            (*handler)(registry, source, target, action);
        }
        break;
    }
    case ActionType::PlaySound: {
        const auto* soundParams = std::get_if<SoundActionParams>(&action.params);
        if (soundParams) {
            events.emit(
                GameplayEvent{GameplayEvent::Kind::PlaySound, soundParams->soundId,
                              soundParams->volume});
        }
        break;
    }
    case ActionType::SetLight: {
        if (target == entt::null) break;
        auto* light = registry.try_get<LightComponent>(target);
        if (!light) break;
        const auto* lightParams = std::get_if<LightActionParams>(&action.params);
        if (!lightParams) break;
        if (lightParams->intensity != 0.0f) {
            light->intensity = lightParams->intensity;
        }
        if (lightParams->radius != 0.0f) {
            light->radius = lightParams->radius;
        }
        const glm::vec3 zero(0.0f);
        if (lightParams->color != zero) {
            light->color = lightParams->color;
        }
        break;
    }
    case ActionType::FlickerLight: {
        // FlickerLight: simple implementation - toggle intensity rapidly
        // Future: could use a dedicated FlickerState component
        if (target == entt::null) break;
        auto* light = registry.try_get<LightComponent>(target);
        if (!light) break;
        // Toggle between full and dim to simulate flicker
        light->intensity = (light->intensity > 0.5f) ? 0.1f : light->intensity * 10.0f;
        break;
    }
    case ActionType::ShowMessage: {
        const auto* msgParams = std::get_if<MessageActionParams>(&action.params);
        if (msgParams) {
            events.emit(GameplayEvent{GameplayEvent::Kind::ShowMessage, msgParams->text,
                                      msgParams->duration});
        }
        break;
    }
    case ActionType::Delay: {
        // Delay actions are handled in executeActionList — they should not reach here
        break;
    }
    case ActionType::EnableEntity: {
        if (target == entt::null) break;
        if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
            interactable->enabled = true;
        }
        break;
    }
    case ActionType::DisableEntity: {
        if (target == entt::null) break;
        if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
            interactable->enabled = false;
        }
        break;
    }
    case ActionType::EmitEvent: {
        const auto* eventParams = std::get_if<EventActionParams>(&action.params);
        if (eventParams) {
            events.emit(
                GameplayEvent{GameplayEvent::Kind::Custom, eventParams->eventName, 0.0f});
        }
        break;
    }
    case ActionType::LockPlayer: {
        const auto* lockParams = std::get_if<PlayerLockParams>(&action.params);
        auto lockView = registry.view<PlayerInteractionLockComponent>();
        for (auto [lockEntity, lock] : lockView.each()) {
            lock.active = true;
            if (lockParams && lockParams->duration > 0.0f) {
                lock.remainingTime = lockParams->duration;
            } else {
                lock.remainingTime = 0.0f;  // indefinite until UnlockPlayer
            }
            break;
        }
        break;
    }
    case ActionType::UnlockPlayer: {
        auto lockView = registry.view<PlayerInteractionLockComponent>();
        for (auto [lockEntity, lock] : lockView.each()) {
            lock.active = false;
            lock.remainingTime = 0.0f;
            break;
        }
        break;
    }
    case ActionType::TeleportPlayer: {
        const auto* teleportParams = std::get_if<TeleportPlayerParams>(&action.params);
        if (!teleportParams) break;
        auto playerView = registry.view<PlayerTag, TransformComponent>();
        for (auto [playerEntity, transform] : playerView.each()) {
            transform.position = teleportParams->position;
            break;
        }
        break;
    }
    }
}

void GameplayBehaviors::schedulePendingAction(entt::entity source, const std::string& eventType,
                                              std::size_t actionIndex, float fireTime) {
    GameplayPendingAction pending;
    pending.fireTime = fireTime;
    pending.source = source;
    pending.eventType = eventType;
    pending.actionIndex = actionIndex;

    // Insert sorted by fireTime using lower_bound
    auto it =
        std::lower_bound(pendingActions_.begin(), pendingActions_.end(), pending,
                         [](const GameplayPendingAction& a, const GameplayPendingAction& b) {
                             return a.fireTime < b.fireTime;
                         });
    pendingActions_.insert(it, pending);
}
