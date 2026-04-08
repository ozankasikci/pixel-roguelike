#include "game/modules/door/DoorActionHandler.h"

#include "game/modules/door/DoorComponents.h"
#include "game/behavior/ActionTypes.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"

void handleDoorAction(GameRegistry& registry,
                      entt::entity /*source*/,
                      entt::entity target,
                      ActionEntry& action) {
    switch (action.type) {
    case ActionType::OpenDoor: {
        if (target == entt::null) break;
        auto* config = registry.try_get<DoorConfigComponent>(target);
        auto* state = registry.try_get<DoorStateComponent>(target);
        if (!config || !state) break;
        if (isDoorFullyClosed(*state)) {
            state->targetState = DoorTargetState::Open;
            state->progress = 0.0f;
            const auto* doorParams = std::get_if<DoorActionParams>(&action.params);
            if (doorParams && doorParams->duration > 0.0f) {
                config->openDuration = doorParams->duration;
            }
            if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
                interactable->busy = true;
            }
            // Lock player during door open animation
            for (auto [lockEntity, lock] : registry.view<PlayerInteractionLockComponent>().each()) {
                if (!lock.active) {
                    lock.active = true;
                    lock.remainingTime = config->openDuration * 0.9f;
                }
                break;
            }
        }
        break;
    }
    case ActionType::CloseDoor: {
        if (target == entt::null) break;
        auto* state = registry.try_get<DoorStateComponent>(target);
        if (!state) break;
        state->targetState = DoorTargetState::Closed;
        state->progress = 0.0f;
        if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
            interactable->busy = false;
            interactable->enabled = true;
        }
        break;
    }
    case ActionType::ToggleDoor: {
        if (target == entt::null) break;
        auto* config = registry.try_get<DoorConfigComponent>(target);
        auto* state = registry.try_get<DoorStateComponent>(target);
        if (!config || !state) break;
        if (isDoorFullyOpen(*state) || isDoorMoving(*state)) {
            // Close it
            state->targetState = DoorTargetState::Closed;
            state->progress = 0.0f;
            if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
                interactable->busy = false;
                interactable->enabled = true;
            }
        } else {
            // Open it
            state->targetState = DoorTargetState::Open;
            state->progress = 0.0f;
            const auto* doorParams = std::get_if<DoorActionParams>(&action.params);
            if (doorParams && doorParams->duration > 0.0f) {
                config->openDuration = doorParams->duration;
            }
            if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
                interactable->busy = true;
            }
            for (auto [lockEntity, lock] : registry.view<PlayerInteractionLockComponent>().each()) {
                if (!lock.active) {
                    lock.active = true;
                    lock.remainingTime = config->openDuration * 0.9f;
                }
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}
