#include "game/modules/checkpoint/CheckpointSystem.h"

#include "game/modules/checkpoint/CheckpointFeedbackState.h"

#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"

#include <algorithm>

void initializeCheckpointFeedback(GameRegistry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
}

void tickCheckpointFeedback(GameRegistry& registry, float deltaTime) {
    auto& ctx = registry.ctx();
    initializeCheckpointFeedback(registry);

    auto& feedback = ctx.get<RuntimeCheckpointFeedbackState>();
    if (feedback.messageTimer > 0.0f) {
        feedback.messageTimer = std::max(0.0f, feedback.messageTimer - deltaTime);
    }

    for (auto [entity, checkpoint] : registry.view<CheckpointComponent>().each()) {
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity);
            light != nullptr && checkpoint.active) {
            light->radius = std::max(light->radius, 10.0f);
            light->intensity = std::max(light->intensity, 2.2f);
        }

        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->promptText = checkpoint.active
                ? "RESPAWN ATTUNED"
                : "E  KINDLE CHECKPOINT";
            interactable->busyText = "CHECKPOINT KINDLED";
            interactable->busy = checkpoint.active && feedback.messageTimer > 0.0f;
        }
    }
}
