#include "game/behavior/TriggerSystem.h"

#include "engine/core/Application.h"
#include "game/behavior/TriggerComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"

#include <cmath>
#include <glm/glm.hpp>

void TriggerSystem::init(Application& /*app*/) {
}

void TriggerSystem::update(Application& app, float /*deltaTime*/) {
    auto& registry = app.registry();

    // Find player entity
    entt::entity playerEntity = entt::null;
    glm::vec3 playerPosition{0.0f};

    auto playerView = registry.view<PlayerTag, TransformComponent>();
    for (auto [entity, transform] : playerView.each()) {
        playerEntity = entity;
        playerPosition = transform.position;
        break;
    }

    if (playerEntity == entt::null) {
        return;
    }

    // Check all trigger volumes for player overlap
    auto triggerView = registry.view<TriggerComponent, TransformComponent>();
    for (auto [entity, trigger, transform] : triggerView.each()) {
        if (!trigger.enabled) {
            continue;
        }

        bool overlapping = false;

        if (trigger.shape == TriggerShape::Box) {
            // AABB overlap check: player within trigger bounds
            const glm::vec3 delta = playerPosition - transform.position;
            overlapping = (std::fabs(delta.x) < trigger.halfExtents.x)
                       && (std::fabs(delta.y) < trigger.halfExtents.y)
                       && (std::fabs(delta.z) < trigger.halfExtents.z);
        } else {
            // Sphere overlap check: distance < radius
            const float distSq = glm::dot(playerPosition - transform.position,
                                          playerPosition - transform.position);
            overlapping = distSq < (trigger.radius * trigger.radius);
        }

        if (overlapping && !trigger.playerInside) {
            trigger.playerInside = true;
            trigger.pendingEnter = true;

            // For fireOnce triggers, disable after first entry
            if (trigger.fireOnce) {
                trigger.enabled = false;
            }
        } else if (!overlapping && trigger.playerInside) {
            trigger.playerInside = false;
            trigger.pendingExit = true;
        }
    }
}

void TriggerSystem::shutdown() {
}
