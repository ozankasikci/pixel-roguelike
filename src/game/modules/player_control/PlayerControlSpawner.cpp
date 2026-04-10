#include "game/modules/player_control/PlayerControlSpawner.h"

#include "game/components/CharacterControllerComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "game/session/RunSession.h"

namespace {

glm::vec3 playerEyeHeightOffset() {
    return glm::vec3(0.0f, CharacterControllerComponent{}.eyeHeight, 0.0f);
}

} // namespace

void syncPlayerSpawnState(const LevelDef& level,
                          const std::string& levelId,
                          RunSession& session) {
    const glm::vec3 eyeHeightOffset = playerEyeHeightOffset();

    if (session.currentLevelId != levelId) {
        session.currentLevelId = levelId;
        if (level.hasPlayerSpawn) {
            session.respawnPosition = level.playerSpawn.position + eyeHeightOffset;
            session.fallRespawnY = level.playerSpawn.fallRespawnY;
        }
        return;
    }

    if (level.hasPlayerSpawn && session.respawnPosition == glm::vec3(0.0f)) {
        session.respawnPosition = level.playerSpawn.position + eyeHeightOffset;
        session.fallRespawnY = level.playerSpawn.fallRespawnY;
    }
}

entt::entity spawnPlayerEntity(LevelBuilder& builder,
                               const LevelDef& level,
                               const RunSession& session) {
    auto& registry = builder.registry();
    const glm::vec3 eyeHeightOffset = playerEyeHeightOffset();
    const glm::vec3 defaultSpawn = level.hasPlayerSpawn
        ? level.playerSpawn.position + eyeHeightOffset
        : glm::vec3(0.0f, 1.6f, 5.4f);
    const glm::vec3 spawnPosition = session.respawnPosition == glm::vec3(0.0f)
        ? defaultSpawn
        : session.respawnPosition;
    const float fallRespawnY = level.hasPlayerSpawn
        ? level.playerSpawn.fallRespawnY
        : session.fallRespawnY;

    const entt::entity player = builder.createTransformEntity(spawnPosition);
    registry.emplace<PlayerTag>(player);
    registry.emplace<ControllableTag>(player);
    registry.emplace<CharacterControllerComponent>(player);
    registry.emplace<PlayerMovementComponent>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);
    registry.emplace<PlayerSpawnComponent>(player,
                                           PlayerSpawnComponent{spawnPosition, fallRespawnY});
    return player;
}
