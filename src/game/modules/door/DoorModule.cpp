#include "game/modules/door/DoorModule.h"

#include "game/modules/door/DoorActionHandler.h"
#include "game/modules/door/DoorSerializer.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorSystem.h"
#include "game/level/LevelDef.h"

void registerDoorModule() {
    // Register door_group keyword parser and serializer (per D-05, D-06, D-07)
    registerLevelDefKeyword("door_group", parseDoorGroup, serializeDoorGroups);

    // Register door action handlers (per D-09)
    registerBehaviorActionHandler(ActionType::OpenDoor, handleDoorAction);
    registerBehaviorActionHandler(ActionType::CloseDoor, handleDoorAction);
    registerBehaviorActionHandler(ActionType::ToggleDoor, handleDoorAction);
}
