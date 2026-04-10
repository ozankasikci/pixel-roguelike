#include "engine/physics/PhysicsSystem.h"
#include "game/behavior/ActionTypes.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/ColliderComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "game/modules/player_control/PlayerControlActionHandler.h"
#include "game/modules/player_control/PlayerControlModule.h"
#include "game/modules/player_control/PlayerControlSpawner.h"
#include "game/session/RunSession.h"

#include <cassert>

int main() {
    registerPlayerControlModule();

    LevelDef level;
    level.hasPlayerSpawn = true;
    level.playerSpawn = LevelPlayerSpawn{
        .position = glm::vec3(1.0f, 1.6f, -3.0f),
        .nodeId = "spawn_root",
        .fallRespawnY = -12.0f,
    };

    RunSession session;
    session.currentLevelId = "old_level";
    syncPlayerSpawnState(level, "new_level", session);
    assert(session.currentLevelId == "new_level");
    assert(session.respawnPosition == glm::vec3(1.0f, 3.2f, -3.0f));
    assert(session.fallRespawnY == -12.0f);

    GameRegistry registry;
    MeshLibrary meshLibrary;
    std::vector<entt::entity> entities;
    LevelBuildContext context{registry, meshLibrary, entities};
    LevelBuilder builder(context);

    const entt::entity player = spawnPlayerEntity(builder, level, session);
    assert(player != entt::null);
    const bool hasPlayerComponents = registry.all_of<TransformComponent,
                                                     CameraComponent,
                                                     CharacterControllerComponent,
                                                     PlayerMovementComponent,
                                                     PlayerInteractionLockComponent,
                                                     PlayerSpawnComponent,
                                                     PlayerTag,
                                                     ControllableTag,
                                                     PrimaryCameraTag>(player);
    assert(hasPlayerComponents);
    assert(registry.get<TransformComponent>(player).position == session.respawnPosition);
    assert(registry.get<PlayerSpawnComponent>(player).fallRespawnY == -12.0f);

    auto ground = registry.create();
    ColliderComponent groundCollider;
    groundCollider.shape = ColliderShape::Box;
    groundCollider.mode = ColliderMode::Solid;
    groundCollider.position = glm::vec3(0.0f, -0.5f, 0.0f);
    groundCollider.halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    registry.emplace<ColliderComponent>(ground, groundCollider);

    PhysicsSystem physics;
    physics.init(registry);
    physics.update(registry, 0.0f);
    registry.ctx().insert_or_assign<PhysicsSystem*>(&physics);

    ActionEntry lockAction;
    lockAction.type = ActionType::LockPlayer;
    lockAction.params = PlayerLockParams{1.5f};
    handleLockPlayerAction(registry, entt::null, entt::null, lockAction);
    assert(registry.get<PlayerInteractionLockComponent>(player).active);
    assert(registry.get<PlayerInteractionLockComponent>(player).remainingTime == 1.5f);

    ActionEntry unlockAction;
    unlockAction.type = ActionType::UnlockPlayer;
    unlockAction.params = PlayerLockParams{};
    handleUnlockPlayerAction(registry, entt::null, entt::null, unlockAction);
    assert(!registry.get<PlayerInteractionLockComponent>(player).active);
    assert(registry.get<PlayerInteractionLockComponent>(player).remainingTime == 0.0f);

    registry.get<PlayerMovementComponent>(player).velocity = glm::vec3(2.0f, 3.0f, 4.0f);

    ActionEntry teleportAction;
    teleportAction.type = ActionType::TeleportPlayer;
    teleportAction.params = TeleportPlayerParams{glm::vec3(4.0f, 1.6f, 2.0f)};
    handleTeleportPlayerAction(registry, entt::null, entt::null, teleportAction);

    const auto& transform = registry.get<TransformComponent>(player);
    const auto& movement = registry.get<PlayerMovementComponent>(player);
    const auto& controller = registry.get<CharacterControllerComponent>(player);
    assert(transform.position == glm::vec3(4.0f, 1.6f, 2.0f));
    assert(movement.velocity == glm::vec3(0.0f));
    assert(physics.getCharacterPosition(player)
           == glm::vec3(4.0f, 1.6f, 2.0f) - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));

    physics.shutdownRuntime();
    return 0;
}
