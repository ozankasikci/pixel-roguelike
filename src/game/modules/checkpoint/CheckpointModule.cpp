#include "game/modules/checkpoint/CheckpointModule.h"

#include "game/modules/checkpoint/CheckpointActionHandler.h"
#include "game/modules/checkpoint/CheckpointSerializer.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorSystem.h"
#include "game/level/LevelDef.h"

void registerCheckpointModule() {
    registerLevelDefKeyword("checkpoint", parseCheckpoint, serializeCheckpoints);
    registerBehaviorActionHandler(ActionType::ActivateCheckpoint, handleCheckpointAction);
}
