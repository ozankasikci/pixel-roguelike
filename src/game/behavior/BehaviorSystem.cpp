#include "game/behavior/BehaviorSystem.h"

#include "engine/core/Application.h"
#include "engine/core/EventBus.h"
#include "engine/core/Time.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorComponent.h"
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
#include <unordered_map>

namespace {

std::unordered_map<ActionType, BehaviorActionHandler>& actionHandlerRegistry() {
    static std::unordered_map<ActionType, BehaviorActionHandler> registry;
    return registry;
}

} // namespace

void registerBehaviorActionHandler(ActionType type, BehaviorActionHandler handler) {
    actionHandlerRegistry()[type] = std::move(handler);
}

BehaviorActionHandler* findBehaviorActionHandler(ActionType type) {
    auto& reg = actionHandlerRegistry();
    auto it = reg.find(type);
    return it != reg.end() ? &it->second : nullptr;
}

void BehaviorSystem::init(Application& /*app*/) {
    // No initialization needed — NodeIndex is populated by LevelLoader
}

void BehaviorSystem::update(Application& app, float /*deltaTime*/) {
    const float currentTime = static_cast<float>(app.time().elapsed());

    processDelayedActions(app, currentTime);
    processNewActivations(app, currentTime);
    processTriggerFlags(app, currentTime);
}

void BehaviorSystem::shutdown() {
    pendingActions_.clear();
}

void BehaviorSystem::processNewActivations(Application& app, float currentTime) {
    auto& registry = app.registry();
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

    executeActionList(app, focus.focused, behavior->onActivate, "activate", currentTime);
    focus.activationConsumed = true;
}

void BehaviorSystem::processTriggerFlags(Application& app, float currentTime) {
    auto& registry = app.registry();

    auto view = registry.view<ColliderComponent, BehaviorComponent>();
    for (auto [entity, collider, behavior] : view.each()) {
        if (collider.mode == ColliderMode::Solid) continue;
        if (collider.pendingEnter) {
            executeActionList(app, entity, behavior.onEnter, "enter", currentTime);
            collider.pendingEnter = false;
        }
        if (collider.pendingExit) {
            executeActionList(app, entity, behavior.onExit, "exit", currentTime);
            collider.pendingExit = false;
        }
    }
}

void BehaviorSystem::processDelayedActions(Application& app, float currentTime) {
    auto& registry = app.registry();

    while (!pendingActions_.empty() && pendingActions_.front().fireTime <= currentTime) {
        PendingAction pending = pendingActions_.front();
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
        executeAction(app, pending.source, action);
        if (action.fireOnce) {
            action.fired = true;
        }
    }
}

void BehaviorSystem::executeActionList(Application& app, entt::entity source,
                                       std::vector<ActionEntry>& actions,
                                       const std::string& eventType, float currentTime) {
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
            executeAction(app, source, entry);
            if (entry.fireOnce) {
                entry.fired = true;
            }
        }
    }
}

void BehaviorSystem::executeAction(Application& app, entt::entity source, ActionEntry& action) {
    auto& registry = app.registry();
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
            app.eventBus().publish(PlaySoundEvent{soundParams->soundId, soundParams->volume});
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
            app.eventBus().publish(ShowMessageEvent{msgParams->text, msgParams->duration});
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
            app.eventBus().publish(BehaviorEvent{eventParams->eventName});
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

void BehaviorSystem::schedulePendingAction(entt::entity source, const std::string& eventType,
                                           std::size_t actionIndex, float fireTime) {
    PendingAction pending;
    pending.fireTime = fireTime;
    pending.source = source;
    pending.eventType = eventType;
    pending.actionIndex = actionIndex;

    // Insert sorted by fireTime using lower_bound
    auto it = std::lower_bound(pendingActions_.begin(), pendingActions_.end(), pending,
        [](const PendingAction& a, const PendingAction& b) {
            return a.fireTime < b.fireTime;
        });
    pendingActions_.insert(it, pending);
}
