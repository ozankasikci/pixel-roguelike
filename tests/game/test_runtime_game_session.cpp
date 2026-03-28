#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/runtime/RuntimeInputState.h"
#include "game/session/RunSession.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <entt/entt.hpp>

#include <cassert>

int main() {
    entt::registry registry;

    auto ground = registry.create();
    StaticColliderComponent groundCollider;
    groundCollider.shape = ColliderShape::Box;
    groundCollider.position = glm::vec3(0.0f, -0.5f, 0.0f);
    groundCollider.halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    registry.emplace<StaticColliderComponent>(ground, groundCollider);

    auto player = registry.create();
    registry.emplace<TransformComponent>(player, TransformComponent{glm::vec3(0.0f, 1.6f, 0.0f)});
    registry.emplace<CameraComponent>(player);
    registry.emplace<PlayerMovementComponent>(player);
    registry.emplace<CharacterControllerComponent>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);
    registry.emplace<PlayerSpawnComponent>(player, PlayerSpawnComponent{glm::vec3(0.0f, 1.6f, 0.0f), -6.0f});
    registry.emplace<PlayerTag>(player);
    registry.emplace<ControllableTag>(player);
    registry.emplace<PrimaryCameraTag>(player);

    PhysicsSystem physics;
    physics.init(registry);
    physics.update(registry, 0.0f);

    RuntimeInputState input;
    RunSession session;
    ContentRegistry content;
    initializeRuntimeInventory(registry);

    const glm::vec3 startPosition = registry.get<TransformComponent>(player).position;
    for (int frame = 0; frame < 12; ++frame) {
        input.reset();
        input.beginFrame();
        input.setCursorLocked(true);
        input.setKeyPressed(GLFW_KEY_W, true);

        physics.update(registry, 1.0f / 60.0f);
        updateRuntimeInventory(registry, input, session, content);
        updateRuntimePlayerMovement(registry, input, physics, 1.0f / 60.0f);
        updateRuntimeCamera(registry, input, 16.0f / 9.0f, 1.0f / 60.0f);
    }

    const glm::vec3 movedPosition = registry.get<TransformComponent>(player).position;
    assert(glm::length(movedPosition - startPosition) > 0.01f);

    input.reset();
    input.beginFrame();
    input.setCursorLocked(true);
    input.setKeyPressed(GLFW_KEY_I, true);
    updateRuntimeInventory(registry, input, session, content);

    assert(registry.ctx().contains<InventoryMenuState>());
    assert(registry.ctx().get<InventoryMenuState>().open);
    assert(!input.isCursorLocked());

    input.reset();
    input.beginFrame();
    input.setCursorLocked(false);
    input.setKeyPressed(GLFW_KEY_ESCAPE, true);
    updateRuntimeInventory(registry, input, session, content);

    assert(!registry.ctx().get<InventoryMenuState>().open);
    assert(input.isCursorLocked());

    physics.shutdownRuntime();
    return 0;
}
