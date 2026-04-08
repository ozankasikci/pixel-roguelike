#include "game/modules/checkpoint/CheckpointActionHandler.h"

#include "game/behavior/ActionTypes.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/runtime/RuntimeGameplay.h"

void handleCheckpointAction(entt::registry& registry,
                            entt::entity /*source*/,
                            entt::entity target,
                            ActionEntry& action) {
    (void)action;

    if (target == entt::null) return;

    auto* checkpoint = registry.try_get<CheckpointComponent>(target);
    if (!checkpoint) return;

    // Already active — nothing to do
    if (checkpoint->active) return;

    checkpoint->active = true;

    // Update player respawn position
    auto playerView = registry.view<PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, spawn] : playerView.each()) {
        (void)entity;
        spawn.respawnPosition = checkpoint->respawnPosition;
        break;
    }

    // Trigger feedback timer
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
    ctx.get<RuntimeCheckpointFeedbackState>().messageTimer = 2.5f;
}
