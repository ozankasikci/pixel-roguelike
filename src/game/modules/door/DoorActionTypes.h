#pragma once

#include <cstdint>

// Door-specific action parameter struct.
// NOTE: ActionType enum values (OpenDoor/CloseDoor/ToggleDoor) remain in
// game/behavior/ActionTypes.h — they are shared interface values. Only
// the params struct lives here.
struct DoorActionParams {
    float duration = 1.2f;
};
