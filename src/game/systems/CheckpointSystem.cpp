#include "game/systems/CheckpointSystem.h"

#include "engine/core/Application.h"
#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InteractionFocusState.h"
#include "game/session/RunSession.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

CheckpointSystem::CheckpointSystem(InputSystem& input)
    : input_(input)
{}

void CheckpointSystem::init(Application& app) {
    (void)app;
}

void CheckpointSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
    auto& focus = ctx.get<InteractionFocusState>();
    auto& session = app.getService<RunSession>();

    if (messageTimer_ > 0.0f) {
        messageTimer_ = std::max(0.0f, messageTimer_ - deltaTime);
    }

    PlayerSpawnComponent* playerSpawn = nullptr;

    auto playerView = registry.view<PlayerSpawnComponent>();
    for (auto [entity, spawn] : playerView.each()) {
        playerSpawn = &spawn;
        break;
    }

    if (!playerSpawn) {
        return;
    }

    auto checkpointView = registry.view<TransformComponent, CheckpointComponent>();
    for (auto [entity, transform, checkpoint] : checkpointView.each()) {
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity)) {
            light->intensity = checkpoint.active ? 2.2f : 1.15f;
            light->radius = checkpoint.active ? 10.0f : 7.0f;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->promptText = checkpoint.active ? "RESPAWN ATTUNED" : "E  KINDLE CHECKPOINT";
            interactable->busyText = "CHECKPOINT KINDLED";
            interactable->busy = checkpoint.active && messageTimer_ > 0.0f;
        }
    }

    if (focus.focused == entt::null || !focus.activationRequested || focus.activationConsumed) {
        return;
    }

    auto* checkpoint = registry.try_get<CheckpointComponent>(focus.focused);
    if (checkpoint == nullptr || checkpoint->active) {
        return;
    }

    auto checkpointViewAll = registry.view<CheckpointComponent>();
    for (auto [entity, other] : checkpointViewAll.each()) {
        other.active = false;
    }

    checkpoint->active = true;
    playerSpawn->respawnPosition = checkpoint->respawnPosition;
    session.respawnPosition = checkpoint->respawnPosition;
    messageTimer_ = 2.0f;
    focus.activationConsumed = true;

    if (auto* interactable = registry.try_get<InteractableComponent>(focus.focused)) {
        interactable->busy = true;
    }
}

void CheckpointSystem::shutdown() {
}
