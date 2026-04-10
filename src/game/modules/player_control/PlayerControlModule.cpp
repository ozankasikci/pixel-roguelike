#include "game/modules/player_control/PlayerControlModule.h"

#include "game/behavior/BehaviorSystem.h"
#include "game/level/LevelDef.h"
#include "game/modules/player_control/PlayerControlActionHandler.h"
#include "game/modules/player_control/PlayerControlSerializer.h"

void registerPlayerControlModule() {
    registerLevelDefKeyword("player_spawn", parsePlayerSpawn, serializePlayerSpawn);
    registerBehaviorActionHandler(ActionType::LockPlayer, handleLockPlayerAction);
    registerBehaviorActionHandler(ActionType::UnlockPlayer, handleUnlockPlayerAction);
    registerBehaviorActionHandler(ActionType::TeleportPlayer, handleTeleportPlayerAction);
}
