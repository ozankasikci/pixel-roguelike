#include "game/modules/player_control/PlayerControlActionHandler.h"

#include "engine/physics/PhysicsSystem.h"
#include "game/behavior/ActionTypes.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"

namespace {

entt::entity resolvePlayerEntity(GameRegistry& registry, entt::entity target) {
    if (target != entt::null
        && registry.valid(target)
        && registry.all_of<PlayerTag>(target)) {
        return target;
    }

    auto playerView = registry.view<PlayerTag>();
    for (auto entity : playerView) {
        return entity;
    }

    return entt::null;
}

} // namespace

void handleLockPlayerAction(GameRegistry& registry,
                            entt::entity /*source*/,
                            entt::entity target,
                            ActionEntry& action) {
    const entt::entity player = resolvePlayerEntity(registry, target);
    if (player == entt::null) {
        return;
    }

    auto* lock = registry.try_get<PlayerInteractionLockComponent>(player);
    if (lock == nullptr) {
        return;
    }

    const auto* params = std::get_if<PlayerLockParams>(&action.params);
    lock->active = true;
    lock->remainingTime = (params != nullptr && params->duration > 0.0f)
        ? params->duration
        : 0.0f;
}

void handleUnlockPlayerAction(GameRegistry& registry,
                              entt::entity /*source*/,
                              entt::entity target,
                              ActionEntry& /*action*/) {
    const entt::entity player = resolvePlayerEntity(registry, target);
    if (player == entt::null) {
        return;
    }

    auto* lock = registry.try_get<PlayerInteractionLockComponent>(player);
    if (lock == nullptr) {
        return;
    }

    lock->active = false;
    lock->remainingTime = 0.0f;
}

void handleTeleportPlayerAction(GameRegistry& registry,
                                entt::entity /*source*/,
                                entt::entity target,
                                ActionEntry& action) {
    const auto* params = std::get_if<TeleportPlayerParams>(&action.params);
    if (params == nullptr) {
        return;
    }

    const entt::entity player = resolvePlayerEntity(registry, target);
    if (player == entt::null) {
        return;
    }

    if (auto* transform = registry.try_get<TransformComponent>(player)) {
        transform->position = params->position;
    }

    if (auto* movement = registry.try_get<PlayerMovementComponent>(player)) {
        movement->velocity = glm::vec3(0.0f);
        movement->grounded = false;
        movement->jumpHeld = false;
        movement->jumpHoldTimer = 0.0f;
    }

    auto& ctx = registry.ctx();
    if (!ctx.contains<PhysicsSystem*>()) {
        return;
    }

    PhysicsSystem* physics = ctx.get<PhysicsSystem*>();
    if (physics == nullptr || !registry.all_of<CharacterControllerComponent>(player)) {
        return;
    }

    const auto& controller = registry.get<CharacterControllerComponent>(player);
    physics->setCharacterVelocity(player, glm::vec3(0.0f));
    physics->setCharacterPosition(player,
                                  params->position - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));
}
