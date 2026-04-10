#include "game/modules/checkpoint/CheckpointActionHandler.h"

#include "game/modules/checkpoint/CheckpointFeedbackState.h"

#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/session/RunSession.h"

void handleCheckpointAction(GameRegistry& registry,
                            entt::entity /*source*/,
                            entt::entity target,
                            ActionEntry& /*action*/) {
    auto* checkpoint = registry.try_get<CheckpointComponent>(target);
    if (checkpoint == nullptr || checkpoint->active) {
        return;
    }

    for (auto [entity, otherCheckpoint] : registry.view<CheckpointComponent>().each()) {
        otherCheckpoint.active = (entity == target);
    }

    auto playerView = registry.view<PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, spawn] : playerView.each()) {
        (void)entity;
        spawn.respawnPosition = checkpoint->respawnPosition;
        break;
    }

    auto& ctx = registry.ctx();
    if (ctx.contains<RunSession*>()) {
        if (RunSession* session = ctx.get<RunSession*>(); session != nullptr) {
            session->respawnPosition = checkpoint->respawnPosition;
        }
    }

    auto& feedback = ctx.contains<RuntimeCheckpointFeedbackState>()
        ? ctx.get<RuntimeCheckpointFeedbackState>()
        : ctx.emplace<RuntimeCheckpointFeedbackState>();
    feedback.messageTimer = 2.5f;

    if (auto* interactable = registry.try_get<InteractableComponent>(target)) {
        interactable->busy = true;
    }
}
