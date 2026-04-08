#include "game/modules/checkpoint/CheckpointSystem.h"

#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/TransformComponent.h"
#include "game/runtime/RuntimeGameplay.h"

#include <algorithm>

namespace {

RuntimeCheckpointFeedbackState& ensureCheckpointFeedbackState(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
    return ctx.get<RuntimeCheckpointFeedbackState>();
}

} // namespace

void initializeCheckpointFeedback(entt::registry& registry) {
    (void)ensureCheckpointFeedbackState(registry);
}

void tickCheckpointFeedback(entt::registry& registry, float deltaTime) {
    auto& feedback = ensureCheckpointFeedbackState(registry);

    if (feedback.messageTimer > 0.0f) {
        feedback.messageTimer = std::max(0.0f, feedback.messageTimer - deltaTime);
    }

    auto checkpointView = registry.view<TransformComponent, CheckpointComponent>();
    for (auto [entity, transform, checkpoint] : checkpointView.each()) {
        (void)transform;
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity)) {
            light->intensity = checkpoint.active ? 2.2f : 1.15f;
            light->radius = checkpoint.active ? 10.0f : 7.0f;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->promptText = checkpoint.active ? "RESPAWN ATTUNED" : "E  KINDLE CHECKPOINT";
            interactable->busyText = "CHECKPOINT KINDLED";
            interactable->busy = checkpoint.active && feedback.messageTimer > 0.0f;
        }
    }
}
